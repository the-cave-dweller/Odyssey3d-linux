/* odysseyvk — visor/reproductor 3D sin gafas para el Samsung Odyssey 3D (G90XF)
   en Wayland. Parte de control de hardware: lente lenticular, scaler MediaTek,
   iluminador IR y modo del panel. */
#include "ircam.h"
#include "lens.h"
#include "calib.h"
#include "camera.h"
#include "track.h"
#include "atajos.h"
#include "panel.h"
#include "mpris.h"
#include "ajuste.h"
#include "barra.h"
#include "preferencias.h"
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

static const char *rotW(int r) {
  const char *w = Panel::rotationWord(r);
  return w ? w : "desconocida";
}

static const char *kUsage =
    "odysseyvk — 3D sin gafas en el Samsung Odyssey 3D (G90XF) bajo Wayland\n"
    "\n"
    "  odysseyvk play ARCHIVO        reproducir un video side-by-side en 3D sin gafas\n"
    "  odysseyvk test                patron de calibracion (disparidades, barras RGB) sobre el panel\n"
    "  odysseyvk doctor              diagnostico: lente, scaler, camara IR, panel, permisos\n"
    "  odysseyvk weave               escribe un PNG con la trama calculada (sin tocar el panel)\n"
    "  odysseyvk attrs               atributos de trama calculados (para validar el modelo)\n"
    "  odysseyvk texto-detect PPM    que considera texto el apoyo de los rotulos (foto del --volcado)\n"
    "  odysseyvk barra-detect PNG..  en un video dividido, que filas tapa la barra del reproductor\n"
    "  odysseyvk ajustes [--juegos]  el panel flotante de ajustes y juegos (otra vez lo cierra)\n"
    "  odysseyvk ctl lens   [info|auth|on|off|state] [--port /dev/ttyACM0]\n"
    "  odysseyvk ctl scaler [on|off] [--dev /dev/sg1]\n"
    "  odysseyvk ctl ir     [on|off] [--dev /dev/video2]\n"
    "  odysseyvk ctl calib  [--out calib]   leer calibracion de fabrica (camara IR y transformada)\n"
    "  odysseyvk ctl camera [--dev N] [--size 1280x480] [--format MJPG]   probar la camara\n"
    "  odysseyvk ctl vista  [--dev /dev/videoN] [--fps 8] [--segundos 25]  la camara suelta, sin rastreo:\n"
    "                                        vuelca lo que ve a ~/.local/state/odysseyvk/camara.jpg unas 8\n"
    "                                        veces por segundo. Es lo que levanta la casilla Previsualizacion\n"
    "                                        de la ventana de ajustes cuando no hay sesion 3D abierta; cede la\n"
    "                                        camara en cuanto el visor la pide (codigo 3) y se cierra solo.\n"
    "  odysseyvk ctl track  [--segundos 20] [--dump foto.jpg]   rastreo de cabeza (cara y ojos)\n"
    "                     [--foto IMAGEN] revisa la deteccion en un archivo; [--sintetico] prueba la geometria\n"
    "  odysseyvk ctl panel  [status|apply|restore] [--output DP-4] [--hz 120] [--dry] [--sin-despejar]\n"
    "                                        status dice tambien el sitio de la salida; apply mueve lo que se\n"
    "                                        monte encima del panel y restore lo devuelve (apply/restore --dry\n"
    "                                        imprimen el comando de kscreen-doctor completo, con posiciones)\n"
    "  odysseyvk ctl windows [--watch]\n"
    "  odysseyvk ctl ventana [fullscreen|normal|maximizar|minimizar|aparecer|sobre|debajo|libre|salida NOMBRE|cerrar] [--uuid ID]\n"
    "                                        como esta la ventana del video; `salida DP-4` la devuelve al Odyssey\n"
    "  odysseyvk ctl aparece   la ventana del visor otra vez a la vista (para atarla a la tecla PB)\n"
    "  odysseyvk ctl salidas   cada pantalla con su posicion y tamano logicos en el espacio global\n"
    "  odysseyvk ctl apilamiento  el orden arriba/abajo que ve el compositor (el ultimo esta arriba)\n"
    "  odysseyvk ctl capture [--uuid X] [--out F] [--segundos N]\n"
    "  odysseyvk ctl capture-gl [--uuid X] [--out F] [--segundos N]\n"
    "  odysseyvk ctl vkprobe [--gpu N] [--solamente-gpu] [--verboso] [--uuid X] [--out F]\n"
      "                                        muestra y guarda un fotograma leido en Vulkan\n"
    "  odysseyvk ctl notify [--wait N]\n"
    "  odysseyvk ctl flujo [--uuid ID] [--segundos N]  medir si el compositor manda fotogramas nuevos\n"
    "  odysseyvk ctl video  [status|pause|play|toggle]   pausar y reanudar el video sin quitar la capa\n"
    "  odysseyvk ctl overlay [--segundos 12] [--dump F.png] [--titulo T] [--cuerpo T]\n"
    "                                        poner la tarjeta del hub por encima de todo (capa superpuesta)\n"
    "  odysseyvk ctl escritorio [poner|quitar|estado]   el overlay FORZADO en el escritorio del monitor:\n"
    "                                        trama y lente encendidas con patron de prueba, sin video (lo mismo que el boton de la ventana de ajustes)\n"
    "  odysseyvk ctl 3d     [encender|apagar|alternar|estado]  lo mismo que el interruptor ON/OFF de la\n"
    "                                        pastilla (Ctrl+Shift+1 con el visor enfocado)\n"
    "  odysseyvk ctl capa   [ocultar|mostrar|alternar|estado]  quitar o poner la pastilla sin tocar el\n"
    "                                        teclado (es Ctrl+Shift+F1 mandado por fichero; la trama sigue)\n"
    "  odysseyvk ctl legado [encender|apagar|alternar|estado]  el modo de LEGADO: video que ya viene en dos\n"
    "                                        vistas, sin conversor y con la pastilla sencilla (lo mismo que el\n"
    "                                        interruptor de la ventana de ajustes; se recuerda en serve.conf)\n"
    "  odysseyvk ctl ajuste [clave valor ...]  mandar numeros vivos al visor (fase, paso, conv, planos, ...)\n"
    "  odysseyvk ctl ajuste limpiar | fabrica  vaciar el canal | mandar los numeros DE FABRICA (revertir)\n"
    "  odysseyvk ctl atajos [probar|quitar]    los atajos del hub como globales del escritorio (KDE): probarlos sin teclado\n"
    "  odysseyvk-ajustes                la ventana de ajustes: redes disponibles/seleccionadas, guardados y sus numeros\n"
    "  odysseyvk capture3d [--uuid X]   capturar una ventana a pantalla completa y tramarla en 3D\n"
    "  odysseyvk test [--windowed]      patron de prueba con la interfaz (raton y teclas)\n"
    "  odysseyvk serve [--dry]          servicio: avisa cuando algo se pone a pantalla completa y lo pone en 3D\n"
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
    "  --track-mono si|no|auto      cualquier webcam, midiendo el tamano de la cara (sin calibrar)\n"
    "  --track-campo 65  --track-cara 140   campo horizontal (grados) y ancho de cara (mm) de esa via\n"
    "  --texto-panel si|no          apoyar rotulos y subtitulos en el plano de la pantalla\n"
    "  --align CICLOS    desfase de fase inicial   --pitch F  escala de la rampa de fase\n"
    "  --filtro F        pendiente del filtro de la trama (10 de fabrica; 0 corte seco, 3 suave)\n"
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
    bases.push_back(dir + "/../share/odysseyvk");
    bases.push_back("/usr/local/share/odysseyvk");
    bases.push_back("/usr/share/odysseyvk");
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
  std::string nota;
  Camera::keepAwake(dev, &nota);
  std::printf("  energia %s  (%s)\n", Camera::powerState(dev).c_str(), nota.c_str());
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

/* ---------------- la vista suelta de la camara: `odysseyvk ctl vista` ---------------------------
   Abre la camara y vuelca el ultimo cuadro a `camara.jpg` unas ocho veces por segundo, sin rastreo y
   sin red. Para que sirve: la casilla de Previsualizacion de la hoja de Camara de la ventana de
   ajustes quiere ensenar lo que ve la camara aunque NO haya ninguna sesion 3D abierta, y el unico que
   suele abrir la camara es el visor. La ventana levanta este proceso de hijo y lo cierra al desmarcar
   la casilla, y aqui hay tres salidas para no dejar la camara agarrada nunca:
     - si el visor esta escribiendo su `estado` y esa sesion quiere la camara (`camara_pedida = si`) se
       retira con codigo 3: asi la vista previa le devuelve la camara al rastreo en medio segundo, y el
       visor la reabre (lo intenta unas veces al arrancar la sesion);
     - si le cierran la entrada estandar, o le mandan cualquier tecla, para al instante (se va incluso
       si a la ventana que lo levanto se le muere el proceso);
     - y para solo a los `--segundos` (25 por defecto), por si acaso.
   El cuadro se guarda tal cual lo suelta la camara (MJPG). Si la camara no sabe de JPEG se pasa de
   YUYV a PPM, que la ventana lo lee sin decodificadores. */
/* Que hizo el visor con la camara, leido de su fichero `estado`: cuantos segundos hace que lo toco (-1
   si nunca) y si esa sesion quiere la camara (`camara_pedida = si`). La edad sola no basta para ceder:
   puede haber una sesion abierta sin rastreo (`rastreo = no` en serve.conf) y ahi la camara esta libre,
   asi que la vista previa sigue siendo bienvenida. Y esperar a que diga `facetracking = abierto`
   tampoco sirve: si la camara la tenemos nosotros, el visor nunca logra abrirla y jamas lo diria, o sea
   que las dos partes se esperarian para siempre. Se cede por INTENCION, no por exito. */
static void estadoVisor(double *edad, bool *quiereCamara) {
  if (edad) *edad = -1.0;
  if (quiereCamara) *quiereCamara = false;
  const std::string ruta = ajuste::rutaEstado();
  struct stat st{};
  if (stat(ruta.c_str(), &st) != 0) return;   /* no existe: nunca hubo visor */
  const double ahora = (double)std::chrono::duration<double>(
                           std::chrono::system_clock::now().time_since_epoch()).count();
  if (edad) *edad = ahora - (double)st.st_mtim.tv_sec - (double)st.st_mtim.tv_nsec / 1e9;
  std::ifstream in(ruta);
  std::string linea;
  while (std::getline(in, linea)) {
    if (linea.rfind("camara_pedida = ", 0) == 0) {
      if (quiereCamara) *quiereCamara = linea.compare(16, 2, "si") == 0;
      break;
    }
  }
}

/* YUYV422 -> PPM (P6). La camara del monitor manda JPEG, pero una webcam vieja no, y la ventana de
   ajustes tiene que poder pintar algo sin decodificadores. Devuelve vacio si el cuadro no cuadra. */
static std::vector<uint8_t> yuyvAPpm(const uint8_t *p, size_t size, int w, int h) {
  if (w <= 0 || h <= 0 || size < (size_t)w * (size_t)h * 2u) return {};
  std::vector<uint8_t> out;
  out.reserve((size_t)w * (size_t)h * 3u + 64u);
  char cabecera[64];
  const int largo = std::snprintf(cabecera, sizeof cabecera, "P6\n%d %d\n255\n", w, h);
  out.insert(out.end(), cabecera, cabecera + largo);
  const auto tope = [](int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); };
  for (int i = 0; i < w * h; i++) {
    const size_t par = (size_t)i * 2u;
    const size_t base = par & ~(size_t)1;
    const int y = p[par], cb = p[base + 1], cr = p[base + 3];
    out.push_back((uint8_t)tope(y + (int)(1.402 * (cr - 128))));
    out.push_back((uint8_t)tope(y - (int)(0.344 * (cb - 128) + 0.714 * (cr - 128))));
    out.push_back((uint8_t)tope(y + (int)(1.772 * (cb - 128))));
  }
  return out;
}

static int cmdVistaCamara(int argc, char **argv) {
  std::string err;
  std::string dev = flag(argc, argv, "--dev");
  if (!pickCameraDevice(dev, &err)) {
    std::fprintf(stderr, "%s\n", err.c_str());
    return 1;
  }
  double fps = std::atof(flag(argc, argv, "--fps").c_str());
  if (fps <= 0.0) fps = 8.0;
  fps = fps < 1.0 ? 1.0 : (fps > 30.0 ? 30.0 : fps);
  double limite = std::atof(flag(argc, argv, "--segundos").c_str());
  if (limite <= 0.0) limite = 25.0;
  if (limite > 600.0) limite = 600.0;
  bool vigilarEntrada = !hasFlag(argc, argv, "--sin-entrada");

  /* El modo: JPEG primero (la ventana lo pinta tal cual) y dentro de ese formato el que mas pixeles
     de, sin pasarse; si no sabe de JPEG, YUYV. Los modos enormes se saltean: para una vista previa no
     hace falta traer 4K por el USB. Se consulta la lista ANTES de abrir, igual que hace `ctl camera`:
     consultar y abrir sobre el mismo nodo cerrado es lo que deja a la 04e8:20d7 tranquila. */
  std::vector<Camera::Mode> modos = Camera::modes(dev);
  const auto puntua = [](const Camera::Mode &m) {
    const long bonus = m.fourcc == "MJPG" ? 4000000L : (m.fourcc == "YUYV" ? 0L : -4000000L);
    return bonus + (long)m.width * (long)m.height / 1000L;
  };
  std::sort(modos.begin(), modos.end(),
            [&](const Camera::Mode &a, const Camera::Mode &b) { return puntua(a) > puntua(b); });

  double edad = -1.0;
  bool quiereCamara = false;
  estadoVisor(&edad, &quiereCamara);
  Camera cam;
  if (!cam.open(dev, &err)) {
    if (edad >= 0.0 && edad < 2.5 && quiereCamara)
      std::fprintf(stderr, "%s: hay una sesion 3D abierta y la camara la tiene el visor; marca la "
                           "casilla otra vez cuando se cierre\n", err.c_str());
    else
      std::fprintf(stderr, "%s\n", err.c_str());
    return 1;
  }
  bool listo = false;
  for (const Camera::Mode &m : modos) {
    if ((long)m.width * (long)m.height > 1920L * 1200L) continue;
    if (cam.setFormat(m.width, m.height, m.fourcc, m.fps, &err)) { listo = true; break; }
  }
  if (!listo) {
    const int fijos[][3] = {{1280, 480, 60}, {1280, 720, 30}, {640, 480, 30}};
    for (const auto &f : fijos) {
      if (cam.setFormat(f[0], f[1], "MJPG", f[2], &err)) { listo = true; break; }
      if (cam.setFormat(f[0], f[1], "YUYV", f[2], &err)) { listo = true; break; }
    }
  }
  if (!listo) {
    std::fprintf(stderr, "ningun modo de %s sirvio para la vista: %s\n", dev.c_str(), err.c_str());
    return 1;
  }
  if (!cam.start(&err)) {
    std::fprintf(stderr, "arranque del flujo: %s\n", err.c_str());
    return 1;
  }
  std::printf("vista      %s %dx%d %s · %g cuadros por segundo · hasta %g s\n", dev.c_str(), cam.width(),
              cam.height(), cam.mjpeg() ? "MJPG" : "YUYV", fps, limite);
  std::fflush(stdout);

  const auto t0 = std::chrono::steady_clock::now();
  const auto instantes = [&t0]() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  };
  long long leidos = 0, volcados = 0, sinCuadros = 0;
  double marcaCuadro = -1e9, marcaInfo = -1e9, marcaCeso = -1e9;
  std::string nota;
  int salida = 0;
  for (;;) {
    const double visto = instantes();
    if (visto >= limite) { nota = "se acabo el tiempo pedido"; break; }
    if (vigilarEntrada) {
      /* La entrada es el boton de apagar: la ventana que levanto este proceso le escribe una linea o
         le cierra la tuberia cuando desmarca la casilla, y ahi el proceso se va solo, soltando la
         camara en su propio cierre. Un fin de archivo no siempre es una orden: si la entrada no es ni
         una tuberia ni una terminal (arrancado con la entrada cerrada o apuntando a /dev/null) no hay
         nadie que pueda escribir, y se deja de mirar la entrada en vez de irse a los cero segundos. */
      struct pollfd p{0, POLLIN, 0};
      const int pr = poll(&p, 1, 0);
      if (pr > 0 && (p.revents & (POLLIN | POLLHUP | POLLERR))) {
        char descarte = 0;
        const ssize_t leido = (p.revents & POLLIN) ? ::read(0, &descarte, 1) : 0;
        if (leido > 0) { nota = "me mandaron algo por la entrada: suelto la camara"; break; }
        /* se acabo la entrada. Si era una tuberia o una terminal, eso ES una orden: la ventana que
           levantamos cerro su lado (o alguien le dio Ctrl-D), y hay que soltar la camara. Si era
           /dev/null o un fichero, simplemente no hay nadie delante que pueda mandar nada. */
        struct stat stEntrada;
        const bool delante = fstat(0, &stEntrada) == 0 && (S_ISFIFO(stEntrada.st_mode) || isatty(0));
        if (delante) {
          nota = "se cerro la entrada (se fue la ventana que nos levanto): suelto la camara";
          break;
        }
        vigilarEntrada = false;
        std::printf("vista      la entrada no es una tuberia ni una terminal: no hay boton de apagar por ahi\n");
        std::fflush(stdout);
      }
    }
    if (visto - marcaCeso >= 0.4) {
      marcaCeso = visto;
      double edad = -1.0;
      bool quiereCamara = false;
      estadoVisor(&edad, &quiereCamara);
      if (edad >= 0.0 && edad < 2.5 && quiereCamara) {
        nota = "el visor quiere la camara para su sesion: le cedo";
        salida = 3;
        break;
      }
    }
    const uint8_t *data = nullptr;
    size_t size = 0;
    std::string why;
    if (!cam.read(&data, &size, 400, &why)) {
      if (++sinCuadros > 20) { nota = "la camara dejo de mandar cuadros: " + why; salida = 1; break; }
      /* a los dos segundos sin un solo cuadro se avisa el porque: si la camara no manda nada, eso es lo
         que hay que leer, y no hace falta esperar al resumen del final (ni a los ocho segundos del corte) */
      if (sinCuadros == 5) {
        std::printf("vista      %s no mando ningun cuadro en dos segundos: %s\n", dev.c_str(), why.c_str());
        std::fflush(stdout);
      }
      continue;
    }
    sinCuadros = 0;
    leidos++;
    if (visto - marcaCuadro >= 1.0 / fps) {
      marcaCuadro = visto;
      const std::vector<uint8_t> cuadro =
          cam.mjpeg() ? std::vector<uint8_t>(data, data + size) : yuyvAPpm(data, size, cam.width(), cam.height());
      if (cuadro.empty())
        nota = "el cuadro no tiene el tamano esperado";
      else if (!ajuste::escribirCamaraVista(cuadro, &err))
        nota = err;
      else
        volcados++;
    }
    cam.release();
    if (visto - marcaInfo >= 1.0) {
      marcaInfo = visto;
      const std::string colofon = nota.empty() ? std::string() : " · " + nota;
      std::printf("vista      %lld cuadros, %lld guardados · %.1f por segundo%s\n", leidos, volcados,
                  leidos / std::max(visto, 0.001), colofon.c_str());
      std::fflush(stdout);
    }
  }
  cam.stop();
  std::printf("vista      parada: %lld cuadros, %lld guardados · %s\n", leidos, volcados,
              nota.empty() ? "sin motivo" : nota.c_str());
  std::fflush(stdout);
  return salida;
}

#ifdef HAVE_OPENCV
/* Una linea por cuadro con todo lo que mide el rastreo (v0.74): con esto se ve el ruido de cada
   pieza (los pixeles de cada ojo en cada vista, el punto 3D de cada ojo, la pose cruda y la
   filtrada) sin tener que adivinarlo de la linea por segundo del visor. */
static void csvCabecera(FILE *f) {
  std::fprintf(f, "t,cara,valida,mantenida,camX,camY,camZ,crudoX,crudoY,crudoZ,panelX,panelY,panelZ,ipd,error,"
                  "dIzqX,dIzqY,dDerX,dDerY,iIzqX,iIzqY,iDerX,iDerY,dX,dY,dZ,iX,iY,iZ,"
                  "fino,parFranja,parDisp,mIzqX,mIzqY,mDerX,mDerY\n");
}
static void csvLinea(FILE *f, double t, const TrackSample &m) {
  std::fprintf(f, "%.4f,%d,%d,%d,%.2f,%.2f,%.2f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.2f,%.2f", t, m.detected ? 1 : 0,
               m.accepted ? 1 : 0, m.predicted ? 1 : 0, m.eyeMillimetres[0], m.eyeMillimetres[1],
               m.eyeMillimetres[2], m.rawPanelCentimetres[0], m.rawPanelCentimetres[1],
               m.rawPanelCentimetres[2], m.panelCentimetres[0], m.panelCentimetres[1], m.panelCentimetres[2],
               m.ipdMillimetres, m.reprojectionPx);
  for (int e = 0; e < 2; e++)
    for (int k = 0; k < 4; k++) std::fprintf(f, ",%.2f", m.ojoPx[e][k]);
  for (int e = 0; e < 2; e++)
    for (int k = 0; k < 3; k++) std::fprintf(f, ",%.2f", m.ojoMm[e][k]);
  std::fprintf(f, ",%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n", m.fino ? 1 : 0, m.puntajeFranja, m.puntajeDisparidad,
               m.medioPx[0], m.medioPx[1], m.medioPx[2], m.medioPx[3]);
}

static int cmdTrack(int argc, char **argv) {
  TrackSettings set;
  /* --mono: si = usar la webcam normal por la via de una sola vista; no = exigir
     las dos vistas de la camara del monitor; auto (por defecto) = mirar el
     dispositivo. La via de una sola vista no necesita calibracion de fabrica. */
  const std::string pedidoMono = flag(argc, argv, "--mono").empty() ? "auto" : flag(argc, argv, "--mono");
  if (!flag(argc, argv, "--campo").empty()) set.hFovDegrees = atof(flag(argc, argv, "--campo").c_str());
  if (!flag(argc, argv, "--cara").empty()) set.faceWidthMillimetres = atof(flag(argc, argv, "--cara").c_str());
  if (hasFlag(argc, argv, "--mono-simulacro")) {
    /* La cuenta de la via mono, sin camara ni red: una cara de 150 mm a 650 mm
       con el campo de una webcam normal. Si la inversa no devuelve lo mismo, la
       formula o las unidades se fueron de sitio. */
    std::printf("via mono sin camara (cara de %.0f mm, campo de %.0f grados)\n",
                set.faceWidthMillimetres, set.hFovDegrees);
    std::printf("ancho de imagen  cara medida   distancia recuperada\n");
    bool ok = true;
    for (const int ancho : {640, 1280}) {
      for (const double dist : {400.0, 650.0, 900.0}) {
        double px = 0.0, rec = 0.0;
        HeadTracker::monoSimular(set.hFovDegrees, ancho, set.faceWidthMillimetres, dist, &px, &rec);
        const double error = std::fabs(rec - dist);
        if (error > 0.01) ok = false;
        std::printf("      %4d px        %6.1f px          %8.1f mm  (error %.3f)\n", ancho, px, rec, error);
      }
    }
    std::printf("%s\n", ok ? "la cuenta cuadra" : "MAL: la distancia no vuelve");
    return ok ? 0 : 1;
  }
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
  if (!flag(argc, argv, "--realce").empty()) set.realce = flag(argc, argv, "--realce") != "no";
  if (!flag(argc, argv, "--fino").empty()) set.fino = flag(argc, argv, "--fino") != "no";
  if (!flag(argc, argv, "--euro").empty()) {
    /* --euro MIN[;BETA]: el filtro de la pose, para compararlo sobre una grabacion */
    std::string e = flag(argc, argv, "--euro");
    for (char &c : e)
      if (c == ',') c = '.';
    std::sscanf(e.c_str(), "%lf;%lf", &set.euroMinCorte, &set.euroBeta);
    set.euroMinCorteFino = set.euroMinCorte;
    set.euroBetaFino = set.euroBeta;
  }

  std::string err;
  CameraCalibration calib;
  TrackerTransform xform;
  const std::string dir = flag(argc, argv, "--calib-dir");
  bool calibVale = loadFactoryCalib(dir, &calib, &xform, &err);
  const std::string calibFallo = err;
  const bool pedidoFoto = !flag(argc, argv, "--foto").empty();
  /* --grabar DIR guarda cada cuadro tal como lo suelta la camara (y su reloj en tiempos.txt);
     --repetir DIR pasa esa grabacion por el rastreo sin abrir la camara, con los mismos tiempos;
     --csv ARCHIVO deja una linea por cuadro. Asi un cambio del rastreo se mide sobre los MISMOS
     cuadros, antes y despues. */
  const std::string grabar = flag(argc, argv, "--grabar");
  const std::string repetir = flag(argc, argv, "--repetir");
  const std::string csvRuta = flag(argc, argv, "--csv");
  const bool pedidoSintetico = hasFlag(argc, argv, "--sintetico") || !repetir.empty();
  bool mono = false;
  std::string dev = flag(argc, argv, "--dev");
  bool delMonitor = false;
  if (pedidoFoto || pedidoSintetico) {
    /* La foto y la prueba de la geometria trabajan sobre las dos vistas: lo que
       haya enchufado ahora no cambia lo que se esta probando. */
    mono = (pedidoMono == "si" || pedidoMono == "1");
  } else {
    if (!pickCameraDevice(dev, &err)) {
      std::fprintf(stderr, "%s\n", err.c_str());
      return 1;
    }
    delMonitor = Camera::esEstereoDelMonitor(dev);
    if (pedidoMono == "si" || pedidoMono == "1") mono = true;
    else if (pedidoMono == "no" || pedidoMono == "0") mono = false;
    else mono = !delMonitor;   /* auto: cualquier otra webcam va por la cara */
  }
  set.mono = mono;
  if (mono) {
    if (!pedidoFoto && !pedidoSintetico)
      std::printf("camara       %s: una sola vista, la distancia sale del ancho de la cara (no hace falta calibracion)\n",
                  dev.c_str());
  } else if (!calibVale) {
    std::fprintf(stderr, "%s\n", calibFallo.c_str());
    return 1;
  } else {
    std::printf("calibracion  %s\n", dir.empty() ? "calib/" : dir.c_str());
    if (!pedidoFoto && !pedidoSintetico && !delMonitor) {
      std::printf("aviso      %s no es la camara del monitor: sus dos mitades no son dos vistas de lo mismo\n",
                  dev.c_str());
      std::printf("             (usa --mono si, o auto, para rastrear por el tamano de la cara)\n");
    }
  }
  std::printf("modelo       %s\n", set.model.c_str());

  const std::string dump = flag(argc, argv, "--dump");
  const std::string volcar = flag(argc, argv, "--volcar-crudo");
  const std::string foto = flag(argc, argv, "--foto");
  if (pedidoFoto) {
    if (mono) {
      std::fprintf(stderr, "--foto trabaja sobre las dos vistas de la camara del monitor; con una webcam"
                           " normal usa --dev y mira las lineas de rastreo\n");
      return 1;
    }
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
  if (pedidoSintetico && mono) {
    std::fprintf(stderr, "--sintetico prueba la geometria de las dos vistas: no vale --mono si\n");
    return 1;
  }
  if (!repetir.empty()) {
    std::ifstream ft(repetir + "/tiempos.txt");
    std::vector<double> tiempos;
    for (double tt; ft >> tt;) tiempos.push_back(tt);
    if (tiempos.empty()) {
      std::fprintf(stderr, "--repetir: no hay tiempos en %s/tiempos.txt\n", repetir.c_str());
      return 1;
    }
    FILE *csv = csvRuta.empty() ? nullptr : std::fopen(csvRuta.c_str(), "w");
    if (csv) csvCabecera(csv);
    int validas = 0, conCara = 0;
    for (size_t n = 0; n < tiempos.size(); n++) {
      char nombre[32];
      std::snprintf(nombre, sizeof nombre, "/%06zu.jpg", n);
      std::ifstream fc(repetir + nombre, std::ios::binary);
      const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(fc)), std::istreambuf_iterator<char>());
      std::vector<uint8_t> left, right;
      int vw = 0, vh = 0;
      std::string why;
      TrackSample sample;
      if (!bytes.empty() &&
          HeadTracker::decodeFrame(bytes.data(), bytes.size(), 1280, 480, true, left, right, vw, vh, &why)) {
        tracker.fijarReloj(tiempos[n]);
        tracker.process(left.data(), right.data(), vw, vh, sample, &why);
      }
      if (sample.detected) conCara++;
      if (sample.accepted && !sample.predicted) validas++;
      if (csv) csvLinea(csv, tiempos[n], sample);
    }
    if (csv) std::fclose(csv);
    std::printf("%zu cuadros repetidos, %d con cara, %d poses validas\n", tiempos.size(), conCara, validas);
    return 0;
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

  if (dev.empty() && !pickCameraDevice(dev, &err)) {
    std::fprintf(stderr, "%s\n", err.c_str());
    return 1;
  }
  Camera cam;
  if (!cam.open(dev, &err)) {
    std::fprintf(stderr, "%s\n", err.c_str());
    return 1;
  }
  std::printf("energia      %s\n", Camera::powerState(dev).c_str());
  if (!cam.lastNote().empty()) std::printf("             %s\n", cam.lastNote().c_str());
  struct Intento {
    int w, h, fps;
    const char *fourcc;
  };
  Intento listaEstereo[] = {{1280, 480, 60, "MJPG"}, {1280, 480, 15, "YUYV"}, {640, 480, 30, "YUYV"}};
  Intento listaMono[] = {{1280, 720, 30, "MJPG"}, {640, 480, 30, "MJPG"}, {640, 480, 60, "MJPG"},
                         {640, 480, 30, "YUYV"}, {320, 240, 30, "YUYV"}};
  const Intento *intentos = mono ? listaMono : listaEstereo;
  const size_t cuenta = mono ? sizeof listaMono / sizeof listaMono[0] : sizeof listaEstereo / sizeof listaEstereo[0];
  bool listo = false;
  for (size_t numero = 0; numero < cuenta && !listo; numero++) {
    const Intento &it = intentos[numero];
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
  int sinCara = 0, ojosMal = 0, geoMal = 0, porRealce = 0;
  FILE *csvVivo = csvRuta.empty() ? nullptr : std::fopen(csvRuta.c_str(), "w");
  if (csvVivo) csvCabecera(csvVivo);
  std::ofstream tiemposGrabados;
  if (!grabar.empty()) tiemposGrabados.open(grabar + "/tiempos.txt");
  bool dumpHecho = false;
  double marcaCuadro = -10.0;   /* el ultimo cuadro crudo que se guardo para la vista */
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
    const double tCuadro = std::chrono::duration<double>(now - t0).count();
    if (!grabar.empty() && cam.mjpeg()) {
      char nombre[32];
      std::snprintf(nombre, sizeof nombre, "/%06d.jpg", leidos - 1);
      std::ofstream fc(grabar + nombre, std::ios::binary);
      fc.write((const char *)data, (std::streamsize)size);
      tiemposGrabados << std::fixed << tCuadro << "\n";
    }
    std::vector<uint8_t> left, right;
    /* el sondeo deja el ultimo cuadro crudo en el mismo sitio que el visor (camara.jpg), asi la
       hoja de camara de la ventana tiene algo que ensenar sin tener que montar el overlay ni
       abrir la camara del monitor dos veces */
    const double vistos = std::chrono::duration<double>(now - t0).count();
    if (vistos - marcaCuadro >= 2.0) {
      marcaCuadro = vistos;
      std::string errVista;
      if (!ajuste::escribirCamaraVista(std::vector<uint8_t>(data, data + size), &errVista))
        std::fprintf(stderr, "aviso: %s\n", errVista.c_str());
    }
    int vw = 0, vh = 0;
    bool dec = false;
    if (mono) {
      dec = HeadTracker::decodeMono(data, size, cam.width(), cam.height(), cam.mjpeg(), left, &why);
      vw = cam.width();
      vh = cam.height();
    } else {
      dec = HeadTracker::decodeFrame(data, size, cam.width(), cam.height(), cam.mjpeg(), left, right, vw,
                                     vh, &why);
    }
    if (!dec) {
      cam.release();
      std::fprintf(stderr, "decodificacion: %s\n", why.c_str());
      continue;
    }
    const uint8_t *lp = left.data(), *rp = right.data();
    /* --volcar-crudo: anota el PRIMER cuadro vivo aunque no haya cara, para comparar el
       cuadro que llega por V4L2 con el mismo cuadro salvado a disco */
    if (!volcar.empty() && (leidos == 1 || leidos == 30)) {
      double brillo = 0, saturados = 0;
      HeadTracker::brilloDeVista(left.data(), vw, vh, &brillo, &saturados);
      std::printf("cuadro %-4zu %zu bytes, vista %dx%d, mjpeg=%d, brillo %.1f, saturados %.1f%%\n",
                  leidos, size, vw, vh, cam.mjpeg() ? 1 : 0, brillo, saturados);
      std::string d3;
      const std::string png = volcar + "." + std::to_string(leidos) + ".png";
      if (HeadTracker::saveAnnotated(lp, rp, vw, vh, set, png, &d3))
        std::printf("anotado vivo %s\n", png.c_str());
      else
        std::fprintf(stderr, "volcar: %s\n", d3.c_str());
      const std::string crudo = volcar + "." + std::to_string(leidos) + ".jpg";
      std::ofstream flujo(crudo, std::ios::binary);
      flujo.write((const char *)data, (std::streamsize)size);
      std::printf("crudo vivo   %s\n", crudo.c_str());
      /* A/B dentro del mismo proceso: el MISMO cuadro, por la via del archivo */
      TrackSample muestra2;
      std::string info2;
      if (HeadTracker::describeStill(crudo, set, volcar + "." + std::to_string(leidos) + ".anot.jpg",
                                     tracker, &muestra2, &info2))
        std::printf("via archivo  %s\n", info2.c_str());
      else
        std::printf("via archivo  fallo: %s\n", info2.c_str());
    }
    TrackSample sample;
    if (mono)
      tracker.processMono(lp, vw, vh, sample, &why);
    else
      tracker.process(lp, rp, vw, vh, sample, &why);
    if (csvVivo) csvLinea(csvVivo, tCuadro, sample);
    if (sample.accepted && !sample.predicted) aceptados++;
    if (!sample.detected) {
      if (sample.reason == "sin cara") sinCara++;
      else if (sample.eyesRejected) ojosMal++;
      else geoMal++;
    }
    if (sample.enhanced) porRealce++;
    if (!dump.empty() && !dumpHecho && sample.detected && !mono) {
      std::string d2;
      if (HeadTracker::saveAnnotated(lp, rp, vw, vh, set, dump, &d2))
        std::printf("anotado      %s\n", dump.c_str());
      else
        std::fprintf(stderr, "anotado: %s\n", d2.c_str());
      dumpHecho = true;
    }
    cam.release();
    if (leidos % mostrarCada == 0) {
      std::string detalle;
      if (mono)
        detalle = sample.faceWidthPx > 0 ? "cara " + std::to_string((int)sample.faceWidthPx) + " px" : "sin cara";
      else {
        char ipd[32];
        std::snprintf(ipd, sizeof ipd, "ipd %5.1f", sample.ipdMillimetres);
        detalle = ipd;
      }
      std::printf("cara %s  panel %7.2f %7.2f %7.2f cm  %-14s  error %4.2f px  %5.1f ms  %s\n",
                  sample.detected ? "si " : "no ", sample.panelCentimetres[0],
                  sample.panelCentimetres[1], sample.panelCentimetres[2], detalle.c_str(),
                  sample.reprojectionPx, sample.latencyMs,
                  sample.accepted ? (sample.predicted ? "mantenida" : "valida")
                                  : (sample.reason.empty() ? "sin usar" : sample.reason.c_str()));
      std::fflush(stdout);
    }
  }
  cam.stop();
  if (csvVivo) std::fclose(csvVivo);
  std::printf("%d fotogramas leidos, %d poses validas\n", leidos, aceptados);
  std::printf("careos       sin cara %d, cara descartada por los ojos %d, geometria rechazada %d"
              " (a las %d las saco el reintento con CLAHE)\n",
              sinCara, ojosMal, geoMal, porRealce);
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
  GLFWwindow *winGl = glfwCreateWindow(320, 240, "odysseyvk-gl", nullptr, nullptr);
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
  if (!n.send("odysseyvk-prueba", "Video a pantalla completa detectado",
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
  n.remove("odysseyvk-prueba");
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
    if (st.geom)
      std::printf("  sitio  : %d,%d  %dx%d logico\n", st.x, st.y, st.w, st.h);
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
    bool sinDespejar = hasFlag(argc, argv, "--sin-despejar");
    int mode = Panel::pickMode(st, hz);
    /* Despejar: si al poner el panel a escala 1 su zona logica crece encima del otro monitor,
       ese monitor se mueve en la misma orden atomica (y su sitio de origen queda guardado). */
    std::vector<Placement> movidas, originales;
    if (!sinDespejar) {
      std::vector<PanelState> todos;
      int nw = 0, nh = 0;
      if (Panel::allOutputs(todos) && Panel::modeSize(st, mode, 1.0, nw, nh)) {
        movidas = Panel::despejar(todos, out, nw, nh);
        for (const Placement &q : movidas) {
          int ox = q.x, oy = q.y;
          for (const PanelState &s : todos)
            if (s.name == q.name && s.geom) {
              ox = s.x; oy = s.y;
              originales.push_back(Placement{s.name, s.x, s.y});
            }
          std::printf("  %s: de %d,%d a %d,%d para no montar %dx%d con el panel nuevo\n", q.name.c_str(),
                      ox, oy, q.x, q.y, nw, nh);
        }
      }
    }
    if (!dry) {
      std::string snapPath;
      if (Panel::saveSnapshot(st, &snapPath, &err, originales.empty() ? nullptr : &originales))
        std::printf("  estado actual guardado en %s (para `ctl panel restore`)\n", snapPath.c_str());
    }
    if (!Panel::apply(out, mode, 1.0, RotNone, true, &err, dry, movidas.empty() ? nullptr : &movidas)) {
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
    std::vector<Placement> otras;
    std::string snapPath;
    if (!Panel::loadSnapshot(snap, &snapPath, &err, &otras)) {
      std::fprintf(stderr, "%s\n", err.c_str());
      return 1;
    }
    std::printf("  instantanea: %s  modo %d %s, escala %g, rotacion %d (%s)\n", snapPath.c_str(), snap.modeIndex,
                snap.modeRes.c_str(), snap.scale, snap.rotation, rotW(snap.rotation));
    for (const Placement &q : otras)
      std::printf("  sitio de origen de %s: %d,%d\n", q.name.c_str(), q.x, q.y);
    if (hasFlag(argc, argv, "--dry")) {
      std::printf("  ejecutaría: kscreen-doctor output.%s.mode.%d output.%s.scale.%g", snap.name.c_str(),
                  snap.modeIndex, snap.name.c_str(), snap.scale);
      if (const char *w = Panel::rotationWord(snap.rotation))
        std::printf(" output.%s.rotation.%s", snap.name.c_str(), w);
      for (const Placement &q : otras)
        std::printf(" output.%s.position.%d,%d", q.name.c_str(), q.x, q.y);
      std::printf("\n");
      return 0;
    }
    if (!Panel::restore(snap, &err, otras.empty() ? nullptr : &otras)) {
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
  /* Los mapas de correccion del panel son de fabrica POR UNIDAD: el de cada G90XF se escribe en su
     propia linea de montaje y solo vale para ESE panel. Los que llevamos instalados son de otra
     unidad; el promedio se parece pero dejan errores de fase localizados (medido: 0,04 ciclos de
     media y hasta 1,7 px de desplazamiento de trama en una esquina). Si el dueno puso los suyos en
     ~/.config/odysseyvk/calib, esos mandan sobre los instalados. */
  if (const char *h = std::getenv("HOME"); h && *h)
    bases.push_back(std::string(h) + "/.config/odysseyvk/calib");
  for (const char *b : {".", "./calib", "../calib"}) bases.push_back(b);
  std::string ed = exeDir();
  if (!ed.empty()) {
    bases.push_back(ed + "/calib");
    bases.push_back(ed + "/../share/odysseyvk/calib");
    bases.push_back(ed + "/../calib");
  }
  bases.push_back("/usr/share/odysseyvk/calib");
  bases.push_back("/usr/local/share/odysseyvk/calib");
  for (const std::string &b : bases) {
    std::string p = b + "/" + name;
    if (fileExists(p)) return p;
  }
  return {};
}

/* Numero escrito con punto o con coma: en esta casa las dos formas son normales y un "0,6" leido
   como 0 habria parecido que el mando nuevo no hace nada. */
static double leerNumero(const char *s) {
  char tmp[64];
  size_t k = 0;
  for (; s[k] && k + 1 < sizeof tmp; k++) tmp[k] = (s[k] == ',') ? '.' : s[k];
  tmp[k] = '\0';
  return std::atof(tmp);
}

static bool parseViewerFlags(int argc, char **argv, ViewerOpts &o) {
  bool enEspera = false;   /* --conversor-en-espera: la red viaja, pero el conversor arranca apagado */
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
    else if (a == "--sin-despejar") o.noLayout = true;
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
    else if (a == "--legado") o.legado = true;
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
    else if (a == "--track-mono") {
      if (!need("track-mono")) return false;
      const std::string v = argv[++i];
      if (v == "si" || v == "1") o.trackMono = 1;
      else if (v == "no" || v == "0") o.trackMono = 0;
      else if (v == "auto") o.trackMono = -1;
      else { std::fprintf(stderr, "--track-mono quiere si, no o auto\n"); return false; }
    }
    else if (a == "--track-campo") { if (!need("track-campo")) return false; o.trackCampo = atof(argv[++i]); }
    else if (a == "--track-cara") { if (!need("track-cara")) return false; o.trackCara = atof(argv[++i]); }
    else if (a == "--texto-panel") {
      if (!need("texto-panel")) return false;
      const std::string v = argv[++i];
      if (v == "si" || v == "1") o.textoPanel = 1;
      else if (v == "no" || v == "0") o.textoPanel = 0;
      else if (v == "auto") o.textoPanel = -1;
      else { std::fprintf(stderr, "--texto-panel quiere si, no o auto\n"); return false; }
    }
    else if (a == "--align") { if (!need("align")) return false; o.align = atof(argv[++i]); }
    else if (a == "--pitch") { if (!need("pitch")) return false; o.pitch = atof(argv[++i]); }
    else if (a == "--filtro") { if (!need("filtro")) return false; o.filtro = atof(argv[++i]); }
    else if (a == "--planos") { if (!need("planos")) return false; o.planos = (float)leerNumero(argv[++i]); }
    else if (a == "--muestras") { if (!need("muestras")) return false; o.muestras = (int)leerNumero(argv[++i]); }
    else if (a == "--mapa-fijo") { if (!need("mapa-fijo")) return false; o.mapaFijo = argv[++i]; }
    else if (a == "--apoyo") { if (!need("apoyo")) return false; o.apoyo = leerNumero(argv[++i]); }
    else if (a == "--relieve") { if (!need("relieve")) return false; o.relieve = (int)leerNumero(argv[++i]); }
    else if (a == "--calib-dir") { if (!need("calib-dir")) return false; o.corrDir = argv[++i]; }
    else if (a == "--volcado-seguir") o.dumpKeep = true;
    else if (a == "--uuid") { if (!need("uuid")) return false; o.captureUuid = argv[++i]; }
    else if (a == "--recordar") {
      if (!need("recordar")) return false;
      const std::string par = argv[++i];
      const size_t dd = par.rfind(':');
      if (dd != std::string::npos) o.recordarApp = par.substr(0, dd), o.recordarModo = par.substr(dd + 1);
    }
    else if (a == "--recordado") o.recordado = true;
    else if (a == "--conversor-en-espera") enEspera = true;
    else if (a == "--sin-suavizado") o.suavizado = false;
    else if (a == "--suavizado-auto") o.suavizadoAuto = true;
    else if (a == "--suave") {
      /* video dividido suavizado: una de sus dos imagenes y el relieve del conversor */
      o.suave = true;
      o.conver = true;
      o.fmtGiven = true;
    }
    else if (a == "--segundos") { if (!need("segundos")) return false; o.segundos = atoi(argv[++i]); }
    else if (a == "--help" || a == "-h") { std::fputs(kUsage, stdout); exit(0); }
    else if (!a.empty() && a[0] == '-') { std::fprintf(stderr, "opcion desconocida: %s\n", a.c_str()); return false; }
    else if (o.file.empty()) o.file = a;
    else { std::fprintf(stderr, "solo se admite un archivo; ignorando %s\n", a.c_str()); }
  }
  if (o.track && o.trackModel.empty()) o.trackModel = defaultModelPath();
  /* `--modelo` y `--alterno` encienden el conversor por si solos (es lo que espera quien los escribe
     a mano). El servicio los pasa tambien con el video dividido, para que el boton de video normal
     de la pastilla abra la red de serve.conf, y ahi el conversor tiene que esperar apagado. */
  if (enEspera) o.conver = false;
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
  if (!o.corrA.empty() && !o.corrB.empty() && !o.noCorr) {
    /* los mapas de correccion son un numero de serie: decir de donde salieron es la unica
       manera de ver si son los de este panel o los genericos que llevamos instalados */
    std::printf("[correccion] de %s y %s\n", o.corrA.c_str(), o.corrB.c_str());
  }
  return true;
}

/* ---------------- atributos de la trama (para validar el modelo) ---------------- */

/* ---------------- barra-detect: la barra del reproductor en un video dividido -----------------
   Lo mismo que hace el visor cada cuatro cuadros, pero con capturas en archivo: cada PNG se reduce
   a 518 x 518 como lo reduce la tarjeta (un punto por celda, interpolado: sin promediar) y pasa por
   el detector, en orden y con un decimo de segundo entre uno y otro, para ver tambien la memoria
   (dos analisis para poner la banda, medio segundo para quitarla). `--filas` ensena el residuo de
   cada fila de las zonas de arriba y de abajo. */
static int cmdBarra(int argc, char **argv) {
  bool filas = false;
  std::vector<std::string> fotos;
  for (int i = 0; i < argc; i++) {
    const std::string a = argv[i];
    if (a == "--filas") filas = true;
    else if (!a.empty() && a[0] != '-') fotos.push_back(a);
  }
  if (fotos.empty()) {
    std::fprintf(stderr, "uso: odysseyvk barra-detect [--filas] CUADRO.png [OTRO.png ...]\n");
    return 2;
  }
  DetectorBarra detector;
  const int lado = 518;
  std::vector<uint8_t> chica((size_t)lado * lado * 4u);
  double t = 0.0;
  for (const std::string &f : fotos) {
    int w = 0, h = 0;
    std::vector<uint8_t> px;
    if (!pngLoadRGBA(f, w, h, px) || w < 64 || h < 64) {
      std::fprintf(stderr, "no se pudo leer %s\n", f.c_str());
      return 1;
    }
    for (int y = 0; y < lado; y++)
      for (int x = 0; x < lado; x++) {
        const float fx = ((float)x + 0.5f) / (float)lado * (float)w - 0.5f, fy = ((float)y + 0.5f) / (float)lado * (float)h - 0.5f;
        const int x0 = std::max(0, std::min(w - 1, (int)std::floor(fx))), y0 = std::max(0, std::min(h - 1, (int)std::floor(fy)));
        const int x1 = std::min(w - 1, x0 + 1), y1 = std::min(h - 1, y0 + 1);
        const float tx = std::max(0.0f, fx - (float)x0), ty = std::max(0.0f, fy - (float)y0);
        for (int c = 0; c < 4; c++) {
          const float a = (float)px[((size_t)y0 * w + x0) * 4u + c] * (1.0f - tx) + (float)px[((size_t)y0 * w + x1) * 4u + c] * tx;
          const float b = (float)px[((size_t)y1 * w + x0) * 4u + c] * (1.0f - tx) + (float)px[((size_t)y1 * w + x1) * 4u + c] * tx;
          chica[((size_t)y * lado + x) * 4u + c] = (uint8_t)std::lround(a * (1.0f - ty) + b * ty);
        }
      }
    DetectorBarra::Medida med;
    const auto antes = std::chrono::steady_clock::now();
    const Filas2D r = detector.analizar(chica.data(), lado, lado, t, &med);
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - antes).count();
    t += 0.1;
    std::printf("%s (%dx%d): %.2f ms · centro %.1f · umbral %.1f · %s · abajo %d filas (max %.0f) · arriba %d filas (max %.0f)"
                "  ->  en 2D: abajo %.1f %% · arriba %.1f %%\n", f.c_str(), w, h, ms, med.centro, med.umbral,
                med.dividido ? "dividido" : "NO dividido", med.filasAbajo, med.maxAbajo, med.filasArriba, med.maxArriba,
                100.0 * r.abajo, 100.0 * r.arriba);
    {
      /* y cuanto separa el propio video sus dos imagenes, en milimetros de este panel (59,8 cm) */
      const auto t0 = std::chrono::steady_clock::now();
      const Separacion sp = medirSeparacion(chica.data(), lado, lado);
      const double msSep = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
      if (sp.valida)
        std::printf("    separacion del video (%.2f ms, %d bloques): normal %+.1f mm · lo mas cercano %+.1f mm · lo mas "
                    "lejano %+.1f mm\n", msSep, sp.bloques, 598.0 * sp.normal, 598.0 * sp.cerca, 598.0 * sp.lejos);
      else
        std::printf("    separacion del video: sin bloques que casen (%d)\n", sp.bloques);
    }
    if (filas) {
      const std::vector<float> res = residuoPorFila(chica.data(), lado, lado, 4);
      for (int y = 0; y < lado; y++)
        if (res[(size_t)y] >= 0.0f && (y < lado * 16 / 100 || y >= lado * 74 / 100))
          std::printf("  fila %3d (%.1f %%)  %6.1f %s\n", y, 100.0 * y / lado, res[(size_t)y],
                      res[(size_t)y] > med.umbral ? "<--" : "");
    }
  }
  return 0;
}

/* ---------------- texto-detect: ajustar el apoyo de los rotulos sin sentarse delante -----------
   El visor detecta las zonas que son texto para apoyarlas en el plano de la pantalla (asi los
   subtitulos no se deslizan al mover la cabeza). Para ver que esta detectando, se le pasa la foto
   pequena que el mismo visor vuelca con --volcado (-chico.ppm) y, si se quiere ver el efecto, el
   mapa del mismo cuadro (-profundidad.pgm). Escribe la rejilla de deteccion y el mapa apoyado. */
static bool leerPpm(const std::string &ruta, int &w, int &h, std::vector<uint8_t> &rgb, std::string *err) {
  FILE *f = std::fopen(ruta.c_str(), "rb");
  if (!f) {
    if (err) *err = "no se pudo abrir " + ruta;
    return false;
  }
  int maxv = 0;
  if (std::fscanf(f, "P6 %d %d %d", &w, &h, &maxv) != 3 || w <= 0 || h <= 0 || maxv != 255) {
    std::fclose(f);
    if (err) *err = "no es un PPM binario (P6) de 8 bits: " + ruta;
    return false;
  }
  std::fgetc(f);   /* el unico espacio que va despues del maximo */
  rgb.assign((size_t)w * (size_t)h * 3, 0);
  const size_t leido = std::fread(rgb.data(), 1, rgb.size(), f);
  std::fclose(f);
  if (leido != rgb.size()) {
    if (err) *err = "el PPM esta truncado: " + ruta;
    return false;
  }
  return true;
}

static bool leerPgm(const std::string &ruta, int &w, int &h, std::vector<uint8_t> &g, std::string *err) {
  FILE *f = std::fopen(ruta.c_str(), "rb");
  if (!f) {
    if (err) *err = "no se pudo abrir " + ruta;
    return false;
  }
  int maxv = 0;
  if (std::fscanf(f, "P5 %d %d %d", &w, &h, &maxv) != 3 || w <= 0 || h <= 0 || maxv != 255) {
    std::fclose(f);
    if (err) *err = "no es un PGM binario (P5) de 8 bits: " + ruta;
    return false;
  }
  std::fgetc(f);
  g.assign((size_t)w * (size_t)h, 0);
  const size_t leido = std::fread(g.data(), 1, g.size(), f);
  std::fclose(f);
  if (leido != g.size()) {
    if (err) *err = "el PGM esta truncado: " + ruta;
    return false;
  }
  return true;
}

static void escribirPgm(const std::string &ruta, int w, int h, const std::vector<uint8_t> &g) {
  FILE *f = std::fopen(ruta.c_str(), "wb");
  if (!f) {
    std::fprintf(stderr, "no se pudo escribir %s\n", ruta.c_str());
    return;
  }
  std::fprintf(f, "P5\n%d %d\n255\n", w, h);
  std::fwrite(g.data(), 1, g.size(), f);
  std::fclose(f);
  std::printf("escrito %s\n", ruta.c_str());
}

/* Mirar los DOS PLANOS de un mapa de profundidad guardado, sin sentarse delante del panel. El visor
   tira el mapa de cada cuadro con `--dump /ruta` (termina en -profundidad.pgm), asi que con esto se
   ve donde caen el fondo y los primeros planos, cuantos puntos van a cada lamina y, si se le pasan
   varios mapas seguidos, cuanto parpadeo hay de un cuadro al siguiente (el "cambio de bando": puntos
   que una pasada estan en la lamina delantera y la siguiente se van atras). Las posiciones de las
   laminas se siguen aqui igual que en el visor, para que lo medido se parezca a lo que pasa de
   verdad en la pelicula. */
static int cmdPlanos(int argc, char **argv) {
  float dureza = 0.6f;
  int muestras = 0;
  bool hist = false;
  std::string out;
  std::vector<std::string> ficheros;
  for (int i = 0; i < argc; i++) {
    const std::string a = argv[i];
    if (a == "--dureza" && i + 1 < argc) dureza = (float)leerNumero(argv[++i]);
    else if (a == "--muestras" && i + 1 < argc) muestras = (int)leerNumero(argv[++i]);
    else if (a == "--out" && i + 1 < argc) out = argv[++i];
    else if (a == "--hist") hist = true;
    else if (a == "--ayuda" || a == "-h") {
      std::printf("odysseyvk planos mapa.pgm [mapa2.pgm ...] [--dureza 0,6] [--muestras 1] "
                  "[--out lamina.pgm] [--hist]\n");
      return 0;
    } else ficheros.push_back(a);
  }
  if (ficheros.empty()) {
    std::fprintf(stderr, "uso: odysseyvk planos mapa.pgm [mapa2.pgm ...] [--dureza 0,6] [--muestras 1] "
                         "[--out lamina.pgm] [--hist]\n");
    return 2;
  }
  float pAtras = -1.0f, pDelante = -1.0f;   /* posiciones seguidas, como en DepthNet */
  char bufTmp[64];
  std::vector<uint8_t> bandaAnterior;
  int wAnt = 0, hAnt = 0;
  double sumaCambio = 0.0;
  int conCambio = 0;
  for (size_t f = 0; f < ficheros.size(); f++) {
    int w = 0, h = 0;
    std::vector<uint8_t> g;
    std::string err;
    if (!leerPgm(ficheros[f], w, h, g, &err)) {
      std::fprintf(stderr, "%s: %s\n", ficheros[f].c_str(), err.c_str());
      return 1;
    }
    const int n = w * h;
    std::vector<float> m((size_t)n);
    for (int i = 0; i < n; i++) m[(size_t)i] = (float)g[(size_t)i] / 255.0f;
    const Planos visto = buscarPlanos(m.data(), n);
    if (hist) {
      int hist2[32] = {0};
      for (int i = 0; i < n; i++) hist2[(int)(m[(size_t)i] * 31.99f)]++;
      int maximo = 1;
      for (int i = 0; i < 32; i++) maximo = std::max(maximo, hist2[i]);
      for (int i = 0; i < 32; i++) {
        const int barras = (int)(40.0 * (double)hist2[i] / (double)maximo);
        std::printf("  %.2f-%.2f |%.*s %d\n", (float)i / 32.0f, (float)(i + 1) / 32.0f, barras,
                    std::string((size_t)barras, '#').c_str(), hist2[i]);
      }
    }
    if (!visto.valido) {
      std::printf("[planos] %d de %d · %d x %d · un solo plano claro: se deja el relieve continuo\n",
                  (int)f + 1, (int)ficheros.size(), w, h);
      pAtras = -1.0f, pDelante = -1.0f;
      bandaAnterior.clear();
      continue;
    }
    if (pAtras < 0.0f || std::fabs(visto.fondo - pAtras) >= 0.15f ||
        std::fabs(visto.frente - pDelante) >= 0.15f) {
      pAtras = visto.fondo;
      pDelante = visto.frente;
    } else {
      pAtras += (visto.fondo - pAtras) * 0.08f;
      pDelante += (visto.frente - pDelante) * 0.08f;
    }
    /* a que lamina va cada punto: el corte cae justo en el medio de las dos posiciones, que es
       tambien donde cruza la franja suave de `aplicarPlanos` */
    const float medio = 0.5f * (pAtras + pDelante);
    std::vector<uint8_t> banda((size_t)n);
    for (int i = 0; i < n; i++) banda[(size_t)i] = m[(size_t)i] > medio ? 1 : 0;
    float cambio = 0.0f;
    if (bandaAnterior.size() == banda.size() && w == wAnt && h == hAnt) {
      int flips = 0;
      for (int i = 0; i < n; i++) flips += banda[(size_t)i] != bandaAnterior[(size_t)i];
      cambio = (float)flips / (float)n;
      sumaCambio += cambio;
      conCambio++;
    }
    bandaAnterior = std::move(banda);
    wAnt = w, hAnt = h;
    Planos seguidas;
    seguidas.valido = true;
    seguidas.fondo = pAtras;
    seguidas.frente = pDelante;
    const float fraccion = aplicarPlanos(m.data(), n, seguidas, dureza, muestras);
    std::printf("[planos] %d de %d · %d x %d · atras %.3f · delante %.3f · separacion %.3f · "
                "%.1f%% del cuadro por delante · cambio de bando %.1f%%%s\n",
                (int)f + 1, (int)ficheros.size(), w, h, pAtras, pDelante, pDelante - pAtras,
                100.0f * fraccion, 100.0f * cambio,
                muestras >= 1 ? (char *)(std::snprintf(bufTmp, sizeof bufTmp, " · %d muestra%s "
                                                       "intermedia%s", muestras, muestras > 1 ? "s" : "",
                                        muestras > 1 ? "s" : ""), bufTmp) : "");
    if (hist) {
      /* el histograma DESPUES de aplastar: ahi se ven las mesetas (los picos del escalon) frente a
         la rampa continua, que es lo que se pregunta con `--muestras` */
      int hist2[32] = {0};
      for (int i = 0; i < n; i++) {
        const float v = m[(size_t)i] < 0.0f ? 0.0f : (m[(size_t)i] > 1.0f ? 1.0f : m[(size_t)i]);
        hist2[(int)(v * 31.99f)]++;
      }
      int maximo = 1;
      for (int i = 0; i < 32; i++) maximo = std::max(maximo, hist2[i]);
      std::printf("  despues de aplastar (dureza %.2f, %d muestra%s):\n", dureza, muestras,
                  muestras > 1 ? "s" : "");
      for (int i = 0; i < 32; i++) {
        if (hist2[i] == 0) continue;
        const int barras = (int)(40.0 * (double)hist2[i] / (double)maximo);
        std::printf("  %.2f-%.2f |%.*s %d\n", (float)i / 32.0f, (float)(i + 1) / 32.0f, barras,
                    std::string((size_t)barras, '#').c_str(), hist2[i]);
      }
    }
    if (!out.empty() && f + 1 == ficheros.size()) {
      std::vector<uint8_t> gris((size_t)n);
      for (int i = 0; i < n; i++) {
        const float v = m[(size_t)i] < 0.0f ? 0.0f : (m[(size_t)i] > 1.0f ? 1.0f : m[(size_t)i]);
        gris[(size_t)i] = (uint8_t)(v * 255.0f + 0.5f);
      }
      escribirPgm(out, w, h, gris);
    }
  }
  if (conCambio > 0)
    std::printf("[planos] parpadeo medio de la costura: %.2f%% de los puntos cambian de lamina "
                "cada cuadro (dureza %.2f)\n",
                100.0f * sumaCambio / (float)conCambio, dureza);
  return 0;
}

/* Pasar fotos reales por la red de profundidad y dejar el mapa en un PGM, sin abrir el panel. Con
   esto se monta el banco de pruebas de lo que hace el conversor con una pelicula de verdad: se
   sacan cuadros con ffmpeg (`ffmpeg -i pelicula.mkv -vf fps=2 cuadro%%04d.png`), se pasan por aqui
   en orden y la salida se mira con `odysseyvk planos cuadro*-mapa.pgm`. La red suaviza en el tiempo
   igual que en la sesion, asi que la secuencia se comporta como en la pelicula. */
/* Leida en glutil.h; se vuelve a declarar aqui para no arrastrar las cabeceras de OpenGL a este
   archivo. Sin libpng devuelve false y ya esta. */
bool pngLoadRGBA(const std::string &path, int &w, int &h, std::vector<uint8_t> &rgba);
/* ---- `odysseyvk actualizar` (v0.95) ------------------------------------------------------------
   Baja la rama main del repositorio a ~/.local/share/odysseyvk/fuente (git), la compila e instala
   con el mismo install.sh de siempre (la parte de root va por pkexec, que pide la clave en una
   ventana), deja apuntada la sesion 3D que haya para que el servicio nuevo la reabra, y reinicia el
   servicio. Corre en su propia unidad de systemd (lo lanza el panel o la tarjeta con systemd-run), asi
   que el reinicio no se lo lleva. Todo lo que dice queda en ~/.local/state/odysseyvk/actualizar.log. */
static void avisoEscritorio(const std::string &titulo, const std::string &cuerpo) {
  const pid_t p = ::fork();
  if (p == 0) {
    ::execlp("notify-send", "notify-send", "-a", "OdysseyVK", "-i", "odysseyvk", titulo.c_str(), cuerpo.c_str(), (char *)nullptr);
    ::_exit(127);
  }
  if (p > 0) {
    int st = 0;
    ::waitpid(p, &st, 0);
  }
}
static std::string comilla(const std::string &s) {
  std::string o = "'";
  for (char c : s) o += (c == '\'') ? std::string("'\\''") : std::string(1, c);
  return o + "'";
}
static int cmdActualizar(int argc, char **argv) {
  bool soloBajar = false;   /* --solo-bajar (taller): bajar y compilar, sin instalar ni reiniciar nada */
  for (int i = 0; i < argc; i++)
    if (std::string(argv[i]) == "--solo-bajar") soloBajar = true;
  const std::string registro = rutaEstadoOdyssey("actualizar.log");
  std::printf("lo que pase queda en %s\n", registro.c_str());
  std::fflush(stdout);
  if (!std::freopen(registro.c_str(), "w", stdout)) return 1;
  ::dup2(1, 2);
  setvbuf(stdout, nullptr, _IOLBF, 0);
  const char *home = std::getenv("HOME");
  const std::string fuente = std::string(home ? home : "/tmp") + "/.local/share/odysseyvk/fuente";
  std::printf("[actualizar] OdysseyVK %s · fuente en %s\n", ODYSSEYVK_VERSION, fuente.c_str());
  avisoEscritorio(idioma::tr("Actualizando OdysseyVK"), idioma::tr("Se baja de GitHub y se compila; luego se pide tu clave para instalar."));
  std::string orden;
  if (fileExists(fuente + "/.git/HEAD"))
    orden = "git -C " + comilla(fuente) + " fetch -q origin main && git -C " + comilla(fuente) + " reset -q --hard origin/main";
  else
    orden = "git clone -q --depth 1 " + std::string(prefs::kRepoGitHub) + " " + comilla(fuente);
  std::printf("[actualizar] %s\n", orden.c_str());
  if (std::system(orden.c_str()) != 0) {
    avisoEscritorio(idioma::tr("OdysseyVK: la actualización falló"), idioma::trf("No se pudo bajar el repositorio (¿hay conexión y git?). Detalle en {}", {registro}));
    return 1;
  }
  std::string nueva;
  readSmallFile(fuente + "/VERSION", nueva);
  nueva = strTrim(nueva);
  std::printf("[actualizar] version bajada: %s\n", nueva.c_str());
  /* la instalacion: compila con todos los nucleos; la parte de root (binario, regla udev, datos) la
     pide pkexec en una ventana del escritorio */
  /* la sesion 3D que haya ahora mismo, para que el servicio nuevo la reabra (install.sh lo reinicia, v0.97) */
  {
    const std::map<std::string, std::string> hub = ajuste::leerEstadoHub();
    const auto ses = hub.find("sesion"), ven = hub.find("ventana"), md = hub.find("sesion_modo");
    if (ses != hub.end() && ses->second == "video" && ven != hub.end() && md != hub.end() && md->second != "-") {
      if (FILE *f = std::fopen(rutaEstadoOdyssey("reabrir").c_str(), "we")) {
        std::fprintf(f, "%s\n%s\n%ld\n", ven->second.c_str(), md->second.c_str(), (long)std::time(nullptr));
        std::fclose(f);
        std::printf("[actualizar] se reabrira %s en %s\n", ven->second.c_str(), md->second.c_str());
      }
    }
  }
  orden = "cd " + comilla(fuente) + " && ODYSSEYVK_SUDO=pkexec bash ./install.sh";
  if (soloBajar) {
    orden = "cmake -S " + comilla(fuente) + " -B " + comilla(fuente + "/build") + " -DCMAKE_BUILD_TYPE=Release > /dev/null && cmake --build " +
            comilla(fuente + "/build") + " -j";
    std::printf("[actualizar] (solo bajar y compilar) %s\n", orden.c_str());
    const int rc = std::system(orden.c_str());
    std::printf("[actualizar] hecho (%d)\n", rc);
    return rc == 0 ? 0 : 1;
  }
  std::printf("[actualizar] %s\n", orden.c_str());
  if (std::system(orden.c_str()) != 0) {
    avisoEscritorio(idioma::tr("OdysseyVK: la actualización falló"), idioma::trf("No se pudo compilar o instalar. Detalle en {}", {registro}));
    return 1;
  }
  /* install.sh ya reinicio el servicio (v0.97); aqui solo se comprueba que corre */
  const int rc = std::system("systemctl --user is-active --quiet odysseyvk-serve.service");
  avisoEscritorio(idioma::trf("OdysseyVK {} instalado", {nueva}), idioma::tr(rc == 0 ? "El servicio se ha reiniciado con la versión nueva." : "Instalado, pero el servicio no corre: systemctl --user restart odysseyvk-serve"));
  std::printf("[actualizar] hecho (%d)\n", rc);
  return rc == 0 ? 0 : 1;
}

/* ---- `odysseyvk desinstalar` (v0.95) ----------------------------------------------------------
   Quita el servicio, el lanzador del menu y el icono, la red de profundidad y la fuente bajada, y por
   pkexec el binario, la regla udev y los datos del sistema. La configuracion y la calibracion de
   ~/.config/odysseyvk se quedan, por si vuelve a instalarse. */
static int cmdDesinstalar(int argc, char **argv) {
  (void)argc, (void)argv;
  const char *home = std::getenv("HOME");
  const std::string casa = home ? home : "/tmp";
  char propio[512] = {0};
  const ssize_t n = ::readlink("/proc/self/exe", propio, sizeof propio - 1);
  std::string bin = n > 0 ? std::string(propio, (size_t)n) : std::string("/usr/local/bin/odysseyvk");
  const std::string bindir = bin.substr(0, bin.rfind('/'));
  const std::string prefijo = bindir.substr(0, bindir.rfind('/'));
  avisoEscritorio(idioma::tr("Desinstalando OdysseyVK"), idioma::tr("Se pide tu clave para quitar el programa del sistema."));
  std::system("systemctl --user disable --now odysseyvk-serve.service 2>/dev/null");
  const std::string conf = std::string(std::getenv("XDG_CONFIG_HOME") ? std::getenv("XDG_CONFIG_HOME") : (casa + "/.config"));
  const std::string datos = std::string(std::getenv("XDG_DATA_HOME") ? std::getenv("XDG_DATA_HOME") : (casa + "/.local/share"));
  std::system(("rm -f " + comilla(conf + "/systemd/user/odysseyvk-serve.service") + " " +
               comilla(datos + "/applications/odysseyvk.desktop") + " " + comilla(datos + "/applications/odysseyvk-play.desktop") +
               "; rm -f " + comilla(datos) + "/icons/hicolor/*/apps/odysseyvk.png; systemctl --user daemon-reload").c_str());
  std::system(("rm -rf " + comilla(datos + "/odysseyvk/modelos") + " " + comilla(datos + "/odysseyvk/fuente")).c_str());
  const std::string raiz = "pkexec sh -c " + comilla("rm -f " + comilla(bindir + "/odysseyvk") + " " + comilla(bindir + "/odysseyvk-ajustes") +
                                                      " /etc/udev/rules.d/70-odysseyvk.rules; rm -rf " + comilla(prefijo + "/share/odysseyvk"));
  const int rc = std::system(raiz.c_str());
  std::system(("rm -rf " + comilla(std::string(std::getenv("XDG_STATE_HOME") ? std::getenv("XDG_STATE_HOME") : (casa + "/.local/state")) + "/odysseyvk")).c_str());
  avisoEscritorio(idioma::tr(rc == 0 ? "OdysseyVK desinstalado" : "OdysseyVK: quedó el programa del sistema"),
                  rc == 0 ? idioma::trf("Tu configuración sigue en {}/odysseyvk.", {conf}) : idioma::trf("No se dio la clave: queda {} y la regla udev.", {bin}));
  return rc == 0 ? 0 : 1;
}

static int cmdMapa(int argc, char **argv) {
  std::string modelo;
  int gpu = 0;
  bool sueltas = false;   /* cada foto por separado: sin arrastrar nada de la anterior */
  std::vector<std::string> ficheros;
  for (int i = 0; i < argc; i++) {
    const std::string a = argv[i];
    if ((a == "--modelo" || a == "--model") && i + 1 < argc) modelo = argv[++i];
    else if (a == "--red" && i + 1 < argc) gpu = std::max(0, std::atoi(argv[++i]));
    else if (a == "--sueltas") sueltas = true;
    else if (a == "--ayuda" || a == "-h") {
      std::printf("odysseyvk mapa foto.png [foto2.png ...] [--modelo F.onnx] [--red 0] [--sueltas]\n"
                  "  sin --sueltas las fotos se tratan como cuadros SEGUIDOS de un video (el mapa se suaviza\n"
                  "  en el tiempo); con --sueltas cada una se mide por separado\n");
      return 0;
    } else ficheros.push_back(a);
  }
  if (ficheros.empty()) {
    std::fprintf(stderr, "uso: odysseyvk mapa foto.png [foto2.png ...] [--modelo F.onnx] [--red 0]\n");
    return 2;
  }
  DepthNet net;
  std::string err;
  char bufTmp[128];
  if (!net.init(modelo, &err, gpu)) {
    std::fprintf(stderr, "no se pudo abrir la red: %s\n", err.c_str());
    std::fprintf(stderr, "(las .onnx viven en ~/.local/share/odysseyvk/modelos/)\n");
    return 1;
  }
  /* las fotos pasan a toda prisa: que el suavizado no lo note. ODYSSEYVK_PASO_FIJO (segundos) dice a
     que ritmo se supone que llegan, para comprobar los suavizados por tiempo sin video. */
  {
    const char *pf = entornoOdyssey("ODYSSEYVK_PASO_FIJO");
    const double v = pf ? std::atof(pf) : 0.0;
    net.setPasoFijo(v > 0.0 ? v : DepthNet::kPasoRef);
  }
  std::printf("[mapa] red %s por %s · salida %d x %d\n", net.modelPath().c_str(), net.device().c_str(),
              net.size(), net.size());
  for (size_t f = 0; f < ficheros.size(); f++) {
    int w = 0, h = 0;
    std::vector<uint8_t> rgba;
    const std::string &ruta = ficheros[f];
    if (ruta.size() > 4 && ruta.compare(ruta.size() - 4, 4, ".png") == 0) {
      if (!pngLoadRGBA(ruta, w, h, rgba)) {
        std::fprintf(stderr, "%s: no se pudo leer el PNG\n", ruta.c_str());
        return 1;
      }
    } else {
      std::vector<uint8_t> rgb;
      if (!leerPpm(ruta, w, h, rgb, &err)) {
        std::fprintf(stderr, "%s: %s\n", ruta.c_str(), err.c_str());
        std::fprintf(stderr, "(mapa lee PNG o PPM binario; convert foto.jpg foto.ppm)\n");
        return 1;
      }
      rgba.resize((size_t)w * (size_t)h * 4);
      for (size_t p = 0, q = 0; p + 2 < rgb.size(); p += 3, q += 4) {
        rgba[q] = rgb[p];
        rgba[q + 1] = rgb[p + 1];
        rgba[q + 2] = rgb[p + 2];
        rgba[q + 3] = 255;
      }
    }
    std::vector<uint8_t> m;
    if (sueltas) net.olvidar();
    if (!net.estimate(rgba.data(), w, h, m, &err)) {
      std::fprintf(stderr, "%s: %s\n", ruta.c_str(), err.c_str());
      return 1;
    }
    const int sz = net.size();
    /* el plano de apoyo y los dos planos salen en la misma linea: es lo que el visor le manda al
       sombreador, medido aqui sin sentarse delante del panel */
    std::vector<float> mf((size_t)sz * (size_t)sz);
    for (size_t i = 0; i < mf.size(); i++) mf[i] = (float)m[i] / 255.0f;
    const Planos p = buscarPlanos(mf.data(), sz * sz);
    std::printf("[mapa] %d de %d %s · %.1f ms · apoyo %.3f · %s\n", (int)f + 1, (int)ficheros.size(),
                ruta.c_str(), net.lastMs(), net.focusPlane(),
                p.valido ? (char *)(std::snprintf(bufTmp, sizeof bufTmp, "dos planos atras %.3f · "
                                                                 "delante %.3f · %.1f%% por delante",
                                                  p.fondo, p.frente, 100.0f * p.fraccion),
                              bufTmp)
                         : "un solo plano claro");
    const size_t punto = ruta.find_last_of('.');
    escribirPgm((punto != std::string::npos ? ruta.substr(0, punto) : ruta) + "-mapa.pgm", sz, sz, m);
  }
  return 0;
}

static int cmdTexto(int argc, char **argv) {
  std::string foto, mapa, salida;
  float plano = 0.45f;
  int celda = 8;
  /* --secuencia A.ppm B.ppm ... : varios cuadros seguidos por el detector CON la memoria del apoyo
     (v0.74), como en el visor; deja la rejilla de cada uno en <cuadro>-rotulos.pgm. Para medir el
     parpadeo sin sentarse delante. */
  std::vector<std::string> secuencia;
  bool enSecuencia = false;
  /* el dispatch pasa los argumentos sin hueco de programa, asi que se recorren desde 0 */
  for (int i = 0; i < argc; i++) {
    const std::string a = argv[i];
    if (a == "--plano" && i + 1 < argc) plano = (float)std::atof(argv[++i]);
    else if (a == "--celda" && i + 1 < argc) celda = std::atoi(argv[++i]);
    else if (a == "--mapa" && i + 1 < argc) mapa = argv[++i];
    else if (a == "--out" && i + 1 < argc) salida = argv[++i];
    else if (a == "--secuencia") enSecuencia = true;
    else if (!a.empty() && a[0] != '-' && enSecuencia) secuencia.push_back(a);
    else if (!a.empty() && a[0] != '-') foto = a;
    else {
      std::fprintf(stderr, "no entiendo %s\n", a.c_str());
      return 2;
    }
  }
  if (!secuencia.empty()) {
    MemoriaTexto memoria;
    for (const std::string &ruta : secuencia) {
      int sw = 0, sh = 0;
      std::string e2;
      std::vector<uint8_t> rgb2;
      if (!leerPpm(ruta, sw, sh, rgb2, &e2)) {
        std::fprintf(stderr, "%s: %s\n", ruta.c_str(), e2.c_str());
        return 1;
      }
      std::vector<uint8_t> rgba2((size_t)sw * (size_t)sh * 4, 255);
      for (size_t p = 0, q = 0; p < rgb2.size(); p += 3, q += 4) {
        rgba2[q] = rgb2[p];
        rgba2[q + 1] = rgb2[p + 1];
        rgba2[q + 2] = rgb2[p + 2];
      }
      std::vector<uint8_t> m2((size_t)sw * (size_t)sh, (uint8_t)(plano * 255.0f + 0.5f)), rej;
      int cw2 = 0, ch2 = 0;
      const float frac2 = anclarTexto(rgba2.data(), sw, sh, m2, plano, celda, &rej, &cw2, &ch2, &memoria);
      std::printf("%s: apoyados %.1f%%\n", ruta.c_str(), frac2 * 100.0f);
      const int es = 8;
      std::vector<uint8_t> g((size_t)cw2 * es * ch2 * es, 0);
      for (int y = 0; y < ch2 * es; y++)
        for (int x = 0; x < cw2 * es; x++)
          g[(size_t)y * cw2 * es + x] = rej[(size_t)(y / es) * cw2 + x / es];
      escribirPgm(ruta + "-rotulos.pgm", cw2 * es, ch2 * es, g);
    }
    return 0;
  }
  if (foto.empty()) {
    std::fprintf(stderr, "uso: odysseyvk texto-detect FOTO.ppm [--plano 0,45] [--celda 8] "
                         "[--mapa MAPA.pgm] [--out ROTULOS.pgm]\n"
                         "     odysseyvk texto-detect --secuencia A.ppm B.ppm ...   (con la memoria del apoyo)\n");
    return 2;
  }
  int w = 0, h = 0;
  std::string err;
  std::vector<uint8_t> rgba;
  std::vector<uint8_t> rgb;
  if (leerPpm(foto, w, h, rgb, &err)) {
    rgba.resize((size_t)w * (size_t)h * 4);
    for (size_t p = 0, q = 0; p < rgb.size(); p += 3, q += 4) {
      rgba[q] = rgb[p];
      rgba[q + 1] = rgb[p + 1];
      rgba[q + 2] = rgb[p + 2];
      rgba[q + 3] = 255;
    }
  } else {
    std::fprintf(stderr, "%s\n", err.c_str());
    std::fprintf(stderr, "(texto-detect lee PPM binario; si viene de otro formato: convert foto.png foto.ppm)\n");
    return 1;
  }
  std::vector<uint8_t> m((size_t)w * (size_t)h, (uint8_t)(plano * 255.0f + 0.5f));
  std::vector<uint8_t> antes;
  if (!mapa.empty()) {
    int dw = 0, dh = 0;
    if (!leerPgm(mapa, dw, dh, m, &err) || dw != w || dh != h) {
      std::fprintf(stderr, "--mapa quiere un PGM (P5) del mismo tamano que la foto (%d x %d): %s\n", w, h,
                   err.c_str());
      return 1;
    }
    antes = m;
  }
  std::vector<uint8_t> rejilla;
  int cw = 0, ch = 0;
  const float frac = anclarTexto(rgba.data(), w, h, m, plano, celda, &rejilla, &cw, &ch);
  std::printf("foto %d x %d · plano de apoyo %.2f · celda %d · rejilla %d x %d\n", w, h, plano, celda, cw, ch);
  std::printf("apoyados en la pantalla: %.1f%% de la rejilla\n", frac * 100.0f);
  for (int y = 0; y < ch; y++) {
    int n = 0;
    for (int x = 0; x < cw; x++)
      if (rejilla[(size_t)y * cw + x] >= 128)
        n++;
    if (!n)
      continue;
    std::printf("  fila %2d (%4.1f%% del alto) %3d celdas ", y, 100.0f * (y + 0.5f) / (float)ch, n);
    for (int x = 0; x < cw; x++)
      std::putchar(rejilla[(size_t)y * cw + x] >= 128 ? '#' : '.');
    std::putchar('\n');
  }
  if (!antes.empty()) {
    int maximo = 0, sumados = 0;
    long diferencia = 0;
    for (size_t i = 0; i < m.size(); i++) {
      const int d = std::abs((int)m[i] - (int)antes[i]);
      if (d > maximo) maximo = d;
      if (d > 6) {
        sumados++;
        diferencia += d;
      }
    }
    std::printf("mapa tocado en %.1f%% de los puntos · el mas movido %d de 255 · media de los movidos %d\n",
                100.0 * (double)sumados / (double)m.size(), maximo,
                sumados ? (int)((double)diferencia / (double)sumados) : 0);
    escribirPgm((mapa.size() > 4 ? mapa.substr(0, mapa.size() - 4) : mapa) + "-apoyado.pgm", w, h, m);
  }
  if (salida.empty() && rejilla.size()) {
    /* la rejilla ampliada x8, para verla en un visor de imagenes encima de la foto */
    const int es = 8;
    std::vector<uint8_t> g((size_t)cw * es * ch * es, 0);
    for (int y = 0; y < ch * es; y++)
      for (int x = 0; x < cw * es; x++)
        g[(size_t)y * cw * es + x] = rejilla[(size_t)(y / es) * cw + x / es];
    escribirPgm(foto + "-rotulos.pgm", cw * es, ch * es, g);
  } else if (!salida.empty() && rejilla.size()) {
    const int es = 8;
    std::vector<uint8_t> g((size_t)cw * es * ch * es, 0);
    for (int y = 0; y < ch * es; y++)
      for (int x = 0; x < cw * es; x++)
        g[(size_t)y * cw * es + x] = rejilla[(size_t)(y / es) * cw + x / es];
    escribirPgm(salida, cw * es, ch * es, g);
  }
  return 0;
}

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
  GLFWwindow *win = glfwCreateWindow(64, 64, "odysseyvk-weave", nullptr, nullptr);
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

/* ---- el canal de ajuste vivo (ver src/ajuste.h) ---------------------------------------------
   El visor se queda el teclado de la pantalla del panel, y la ventana de ajustes (otro proceso)
   habla con el por ficheros. Estos verbos son la misma puerta desde la terminal: sirven para mandar
   un numero o una red sin abrir la ventana, y para leer lo que el visor esta usando y lo que se fue
   cambiando (que es lo que despues se puede repasar en voz alta). No hay socket: son tres ficheros
   en ~/.local/state/odysseyvk, y el visor los mira una vez cada decima de segundo. */
static void imprimirAjusteYEstado() {
  const std::string ajuste = ajuste::textoAjuste();
  if (ajuste.empty())
    std::printf("no hay canal de ajuste escrito (%s)\n", ajuste::rutaAjuste().c_str());
  else
    std::fputs(ajuste.c_str(), stdout);
  std::string est;
  if (readSmallFile(ajuste::rutaEstado(), est)) {
    std::printf("--- lo que el visor esta usando ahora mismo (%s) ---\n", ajuste::rutaEstado().c_str());
    std::fputs(est.c_str(), stdout);
  } else {
    std::printf("--- el visor no ha escrito estado: no hay sesion 3D abierta ---\n");
  }
}

static void imprimirModelos() {
  std::vector<std::string> dirs;
  const char *home = std::getenv("HOME");
  const std::string h(home ? home : "/home");
  dirs.push_back(h + "/.local/share/odysseyvk/modelos");
  dirs.push_back("/run/media/rodolfo/067ABD417ABD2DF5/AnalisisOdyssey/modelos");
  if (const char *e = entornoOdyssey("ODYSSEYVK_MODELOS")) {
    std::string d;
    for (char c : std::string(e)) {
      if (c == ':') { if (!d.empty()) dirs.push_back(d); d.clear(); } else d += c;
    }
    if (!d.empty()) dirs.push_back(d);
  }
  std::string puesto;
  for (const auto &kv : ajuste::leerEstado())
    if (kv.first == "modelo") puesto = kv.second;
  std::printf("redes que hay en el disco (para `ctl ajuste modelo=RUTA` o la ventana de ajustes):\n");
  int cont = 0;
  for (const std::string &d : dirs) {
    std::error_code ec;
    if (!std::filesystem::is_directory(d, ec)) continue;
    for (const auto &ent : std::filesystem::directory_iterator(d, ec)) {
      if (ent.path().extension() != ".onnx") continue;
      const std::string ruta = ent.path().string();
      std::printf("  %s  %lld KB%s\n", ruta.c_str(), (long long)(ent.file_size(ec) / 1024),
                  ruta == puesto ? "   <- puesta ahora mismo" : "");
      cont++;
    }
  }
  if (!cont) std::printf("  (ninguna: baja alguna con tools/descargar-modelo.sh)\n");
}

static int cmdEstado() {
  std::string est;
  if (!readSmallFile(ajuste::rutaEstado(), est)) {
    std::printf("el visor no ha escrito estado: no hay sesion 3D abierta (%s)\n", ajuste::rutaEstado().c_str());
    return 1;
  }
  std::fputs(est.c_str(), stdout);
  return 0;
}

static int cmdModelos() {
  imprimirModelos();
  return 0;
}

static int cmdAjuste(int argc, char **argv) {
  if (argc == 0) { imprimirAjusteYEstado(); return 0; }
  const std::string uno = strLower(argv[0]);
  if (uno == "limpiar" || uno == "borrar") {
    std::string err;
    if (!ajuste::escribir(std::map<std::string, double>(), std::string(), &err)) {
      std::fprintf(stderr, "%s\n", err.c_str());
      return 1;
    }
    std::printf("canal de ajuste vaciado: vuelven a mandar las teclas y la linea de mando\n");
    return 0;
  }
  if (uno == "fabrica" || uno == "revertir" || uno == "reset") {
    /* lo mismo que hace el boton «Revertir los cambios de esta red» de la ventana de ajustes, pero sin
       ventana: manda TODOS los numeros de fabrica al visor y quita el apunte de la red que estaba puesta.
       De fabrica = lo que trae el programa, corregido con lo que dice serve.conf. */
    const std::map<std::string, double> base = ajuste::valoresFabrica();
    std::string err;
    if (!ajuste::escribir(base, std::string(), &err)) {
      std::fprintf(stderr, "%s\n", err.c_str());
      return 1;
    }
    std::printf("mandados los %d numeros de fabrica (la tabla del programa corregida con %s):\n",
                (int)base.size(), ajuste::rutaServeConf().c_str());
    for (const auto &kv : base) std::printf("  %-16s %g\n", kv.first.c_str(), kv.second);
    /* y el apunte de la red que estaba puesta deja de tener sentido */
    const std::map<std::string, std::string> est = ajuste::leerEstado();
    const auto it = est.find("modelo");
    if (it != est.end() && !it->second.empty() && ajuste::leerPerfiles().count(it->second)) {
      ajuste::borrarPerfil(it->second, &err);
      std::printf("quitado el guardado de %s\n", it->second.c_str());
    }
    std::printf("si hay una sesion abierta los toma en una decima; si no, esperan a que se abra\n");
    return 0;
  }
  if (uno == "estado") {
    std::string est;
    if (!readSmallFile(ajuste::rutaEstado(), est)) {
      std::printf("el visor no ha escrito estado: no hay sesion 3D abierta\n");
      return 1;
    }
    std::fputs(est.c_str(), stdout);
    return 0;
  }
  if (uno == "modelos" || uno == "redes") { imprimirModelos(); return 0; }
  std::map<std::string, double> numeros;
  std::map<std::string, std::string> palabras;
  std::string modelo;
  std::string dicho;
  for (int i = 0; i < argc;) {
    /* se puede escribir de las dos maneras: `fase 0,004` o `fase=0,004` (como va en el fichero) */
    std::string k = strLower(argv[i]), valor;
    const size_t eq = k.find('=');
    if (eq != std::string::npos) {
      valor = std::string(argv[i]).substr(eq + 1);
      k = k.substr(0, eq);
      i++;
    } else {
      if (i + 1 >= argc) {
        std::fprintf(stderr, "a %s le falta el valor\n", k.c_str());
        return 2;
      }
      valor = argv[i + 1];
      i += 2;
    }
    if (k == "modelo" || k == "red") {
      modelo = valor;
      dicho += "  modelo = " + modelo + "\n";
      continue;
    }
    if (ajuste::esPalabra(k) && !ajuste::esOrden(k)) {
      /* una orden de palabra para el visor (`dividido_suavizado=no`, `capa=ocultar`...) */
      palabras[k] = valor;
      dicho += "  " + k + " = " + valor + "\n";
      continue;
    }
    if (ajuste::esOrden(k)) {
      /* no es un numero del visor: es una orden para el vigilante (`overlay = 1` pone el overlay del
         escritorio, `overlay = 0` lo quita). Tambien esta `ctl escritorio poner|quitar`, que es lo mismo */
      const double num = leerNumero(valor.c_str());
      numeros[k] = num >= 0.5 ? 1.0 : 0.0;
      dicho += std::string("  ") + k + " = " + (numeros[k] >= 0.5 ? "1" : "0") +
               "   /* overlay del escritorio " + std::string(numeros[k] >= 0.5 ? "PUESTO" : "QUITADO") +
               ": lo atiende el hub, no el visor */\n";
      continue;
    }
    const ajuste::Clave *c = ajuste::buscarClave(k);
    if (!c) {
      std::fprintf(stderr, "no conozco ese numero: %s\n", k.c_str());
      std::fprintf(stderr, "se pueden:");
      for (const ajuste::Clave &cc : ajuste::claves()) std::fprintf(stderr, " %s", cc.nombre);
      std::fprintf(stderr, " (y `modelo RUTA.onnx`, o todo con `=`: `fase=0,004`)\n");
      return 2;
    }
    const double num = leerNumero(valor.c_str());
    double v = num;
    if (v < c->minimo) v = c->minimo;
    if (v > c->maximo) v = c->maximo;
    if (c->entero) v = std::floor(v + 0.5);
    numeros[k] = v;
    char b[32];
    std::snprintf(b, sizeof b, c->entero ? "%.0f" : "%.5f", v);
    if (v != num) dicho += "  (" + k + " topado a " + c->nombre + ": estaba fuera de " + std::to_string(c->minimo) + ".." + std::to_string(c->maximo) + "  )\n";
    dicho += std::string("  ") + c->nombre + " = " + b + "   /* " + c->etiqueta + " */\n";
  }
  std::string err;
  if (!ajuste::escribir(numeros, modelo, palabras, &err)) {
    std::fprintf(stderr, "%s\n", err.c_str());
    return 1;
  }
  std::printf("escrito en %s:\n%s", ajuste::rutaAjuste().c_str(), dicho.c_str());
  std::printf("el visor lo lee en una decima de segundo si esta abierto; si esta cerrado, queda "
              "esperando a la siguiente sesion\n");
  return 0;
}

/* `ctl atajos probar`: registra los atajos globales del hub, le pide al escritorio que dispare cada
   uno (sin teclado) y comprueba que llegan; al acabar los deja inactivos. Asi se sabe si el
   escritorio los acepta sin montar una sesion 3D. `ctl atajos quitar` los deja inactivos sin mas. */
static int cmdAtajos(int argc, char **argv) {
  const std::string uno = argc > 0 ? strLower(argv[0]) : "probar";
  if (uno == "quitar" || uno == "soltar") {
    AtajosGlobales::desactivar();
    std::printf("atajos globales del hub: inactivos (las teclas vuelven a las aplicaciones)\n");
    return 0;
  }
  AtajosGlobales at;
  std::string err;
  if (!at.abrir(&err)) {
    std::printf("no se pudieron registrar: %s\n", err.c_str());
    return 1;
  }
  std::printf("registrados: %s\n", at.resumen().c_str());
  int bien = 0;
  for (const std::string &n : AtajosGlobales::nombres()) {
    const bool mandado = at.invocar(n);
    bool llego = false;
    for (int i = 0; i < 40 && !llego; i++) {   /* hasta 400 ms */
      for (const std::string &p : at.pulsados())
        if (p == n) llego = true;
      if (!llego) usleep(10000);
    }
    std::printf("  %-10s %s\n", n.c_str(), !mandado ? "el escritorio no acepto la orden de prueba"
                                           : (llego ? "llega" : "NO llega"));
    if (llego) bien++;
  }
  at.cerrar();
  std::printf("%d de %zu atajos llegan; quedan inactivos hasta la proxima sesion 3D\n", bien,
              AtajosGlobales::nombres().size());
  return bien == (int)AtajosGlobales::nombres().size() ? 0 : 1;
}

static int cmdPreferencias(int argc, char **argv) {
  int n = 40;
  for (int i = 0; i + 1 < argc; i++)
    if (strLower(argv[i]) == "--ultimos") n = std::atoi(argv[++i]);
  const std::vector<std::string> lineas = ajuste::historial(n);
  if (lineas.empty()) {
    std::printf("todavia no se apunto ningun cambio (%s)\n", ajuste::rutaHistorial().c_str());
    return 0;
  }
  std::printf("ultimos %d cambios guardados en %s:\n", (int)lineas.size(), ajuste::rutaHistorial().c_str());
  for (const std::string &l : lineas) std::printf("  %s\n", l.c_str());
  return 0;
}

/* El overlay FORZADO en el escritorio del monitor: lo mismo que hace el boton de la ventana de
   ajustes. `poner` escribe `overlay = 1` en el fichero de ajuste y el hub monta el visor con su
   patron de trama sobre el escritorio (panel a 3D, lente, pastilla y rastreo); `quitar` escribe
   `overlay = 0` y el hub lo despide por la puerta de adelante. `estado` cuenta lo que esta haciendo
   el hub. Ojo: el fichero de ajuste es una ORDEN y se reescribe ENTERO, asi que esta orden se lleva
   los numeros sueltos que estuvieran escritos (el visor ya los aplico, no los echa de menos). */
static int cmdEscritorio(int argc, char **argv) {
  const std::string uno = argc > 0 ? strLower(argv[0]) : "estado";
  if (uno == "estado" || uno == "status" || uno == "info") {
    const std::map<std::string, std::string> hub = ajuste::leerEstadoHub();
    if (hub.empty()) {
      std::printf("el vigilante no ha escrito nada en %s: no hay `odysseyvk serve` corriendo, o es una\n"
                  "version vieja que aun no cuenta lo suyo (instala la nueva con `make install`)\n",
                  ajuste::rutaHub().c_str());
      return 1;
    }
    for (const auto &kv : hub) std::printf("  %-10s %s\n", kv.first.c_str(), kv.second.c_str());
    /* el hub escribe ahi una vez por segundo: si el fichero viene viejo, el servicio ya no esta y lo
       que cuenta es un recuerdo */
    std::error_code ec;
    const auto mt = std::filesystem::last_write_time(ajuste::rutaHub(), ec);
    if (!ec) {
      const long long viejos = (long long)std::chrono::duration_cast<std::chrono::seconds>(
                                   std::filesystem::file_time_type::clock::now() - mt).count();
      std::printf("  %-10s %lld segundos\n", "hace", viejos);
      if (viejos > 5)
        std::printf("  (viene viejo: el servicio se paro; lo arrancas con `systemctl --user restart odysseyvk-serve`)\n");
    }
    return 0;
  }
  double valor = -1.0;
  if (uno == "poner" || uno == "montar" || uno == "1" || uno == "si") valor = 1.0;
  else if (uno == "quitar" || uno == "bajar" || uno == "0" || uno == "no") valor = 0.0;
  else {
    std::printf("uso: odysseyvk ctl escritorio [poner|quitar|estado]\n");
    return 2;
  }
  std::string err;
  if (!ajuste::escribir({{"overlay", valor}}, std::string(), &err)) {
    std::fprintf(stderr, "%s\n", err.c_str());
    return 1;
  }
  std::printf("orden enviada al vigilante: overlay del escritorio %s\n", valor >= 0.5 ? "PUESTO" : "QUITADO");
  return 0;
}

/* Apagar o encender el 3D desde fuera: es el interruptor ON/OFF de la pastilla mandado por fichero,
   lo mismo que picar la pista con el raton o pulsar Ctrl+Shift+1 con el viso delante. Con la trama
   apagada la ventana sigue en su sitio y el panel vuelve a 2D. */
static int cmd3D(int argc, char **argv) {
  const std::string uno = argc > 0 ? strLower(argv[0]) : "alternar";
  if (uno == "estado" || uno == "status" || uno == "info") {
    const std::map<std::string, std::string> est = ajuste::leerEstado();
    if (est.empty()) {
      std::printf("no hay visor abierto: %s esta vacio o no existe\n", ajuste::rutaEstado().c_str());
      return 1;
    }
    for (const char *k : {"trama", "capa"}) {
      const auto it = est.find(k);
      if (it != est.end()) std::printf("  %-12s %s\n", k, it->second.c_str());
    }
    return 0;
  }
  std::string valor;
  if (uno == "apagar" || uno == "quitar" || uno == "0" || uno == "no" || uno == "off") valor = "apagar";
  else if (uno == "encender" || uno == "poner" || uno == "1" || uno == "si" || uno == "on") valor = "encender";
  else if (uno == "alternar" || uno == "toggle") valor = "alternar";
  else {
    std::printf("uso: odysseyvk ctl 3d [encender|apagar|alternar|estado]\n");
    return 2;
  }
  std::string err;
  if (!ajuste::escribir(std::map<std::string, double>(), std::string(), {{"3d", valor}}, &err)) {
    std::fprintf(stderr, "%s\n", err.c_str());
    return 1;
  }
  std::printf("orden enviada al visor: 3D %s (con el visor enfocado es Ctrl+Shift+1, o picar el"
              " interruptor de la pastilla)\n", valor.c_str());
  return 0;
}

/* El modo de legado, por fichero. Es la misma puerta que el interruptor de la pestana General de la
   ventana de ajustes: la trama pasa a los numeros de fabrica, el conversor que saca dos vistas de una
   queda cerrado y la pastilla se queda con lo justo (interruptor, salida 3D y plano de apoyo). No
   cierra ni abre nada: la sesion que estuviera abierta sigue, solo cambia como se arma la imagen.
   El valor queda escrito en serve.conf, asi que la siguiente sesion arranca igual. */
static int cmdLegado(int argc, char **argv) {
  const std::string uno = argc > 0 ? strLower(argv[0]) : "alternar";
  if (uno == "estado" || uno == "status" || uno == "info") {
    const std::map<std::string, std::string> conf = ajuste::leerServeConf();
    const auto itC = conf.find("legado");
    std::printf("modo de legado pedido en serve.conf: %s\n",
                itC == conf.end() ? "no (los numeros finos de siempre mandan)" : itC->second.c_str());
    const std::map<std::string, std::string> est = ajuste::leerEstado();
    const auto itV = est.find("legado");
    if (itV != est.end())
      std::printf("modo de legado en el visor que esta en la pantalla: %s\n", itV->second.c_str());
    else
      std::printf("modo de legado en el visor: no hay visor leyendo el canal (o es de otra version)\n");
    return 0;
  }
  std::string valor;
  if (uno == "apagar" || uno == "quitar" || uno == "0" || uno == "no" || uno == "off") valor = "apagar";
  else if (uno == "encender" || uno == "poner" || uno == "1" || uno == "si" || uno == "on") valor = "encender";
  else if (uno == "alternar" || uno == "toggle") valor = "alternar";
  else {
    std::printf("uso: odysseyvk ctl legado [encender|apagar|alternar|estado]\n");
    return 2;
  }
  std::string err;
  if (!ajuste::escribir(std::map<std::string, double>(), std::string(), {{"legado", valor}}, &err)) {
    std::fprintf(stderr, "%s\n", err.c_str());
    return 1;
  }
  /* que quede escrito para la proxima sesion: el vigilante lee serve.conf al arrancar cada visor */
  if (valor != "alternar") {
    if (!ajuste::fijarEnServeConf("legado", valor == "encender" ? "si" : "no", &err))
      std::fprintf(stderr, "[legado] en el visor si, pero no se pudo escribir serve.conf: %s\n", err.c_str());
    else
      std::printf("ademas queda escrito en serve.conf (legado = %s) para la proxima sesion\n",
                  valor == "encender" ? "si" : "no");
  } else {
    std::printf("(con `alternar` no se escribe serve.conf: di encender o apagar para que quede para siempre)\n");
  }
  std::printf("orden enviada al visor: legado %s\n", valor.c_str());
  return 0;
}

/* Mostrar u ocultar la pastilla sin tocar el teclado. Es la misma orden que hace la tecla
   Ctrl+Shift+F1 dentro del visor, pero mandada por fichero, y por eso llega aunque el teclado de la
   pantalla se lo haya quedado otra ventana (una pagina a pantalla completa que se apilo encima del
   visor, un juego que hace grab del teclado, o el visor minimizado con `Tab`). La trama y el rastreo
   siguen trabajando: lo unico que se quita es la interfaz. */
/* ---------------- ajustes: el panel flotante (v0.84) ----------------------------------------
   Los ajustes de todos los dias y la biblioteca de juegos, en una tarjeta translucida de esquinas
   redondas que flota en el centro de la pantalla, encima de lo que haya: el escritorio (se abre
   con el icono del sistema) o el video en 3D (con el engranaje de la pastilla; ahi se dibuja al
   doble porque se ve a traves de la lente). Es la misma capa superpuesta de la tarjeta de la
   orilla. Abrirlo cuando ya esta abierto lo cierra. `--juegos` lo abre en esa pestana y
   `--dump F.png` deja su dibujo en un archivo. */
static volatile sig_atomic_t g_cierraPanel = 0;
static void alCerrarPanel(int) { g_cierraPanel = 1; }

static int cmdAjustes(int argc, char **argv) {
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
  bool enJuegos = false, forzarGrande = false;
  std::string volcado, pista;
  for (int i = 0; i < argc; i++) {
    const std::string a = argv[i];
    if (a == "--juegos") enJuegos = true;
    else if (a == "--grande") forzarGrande = true;   /* para probar el tamano del 3D sin video */
    else if (a == "--dump" && i + 1 < argc) volcado = argv[++i];
    else if (a == "--pantalla" && i + 1 < argc) pista = argv[++i];
  }
  std::signal(SIGTERM, alCerrarPanel);
  std::signal(SIGINT, alCerrarPanel);
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
  if (FILE *f = std::fopen(rutaPid.c_str(), "we")) {
    std::fprintf(f, "%ld\n", (long)::getpid());
    std::fclose(f);
  }
  /* con un video en 3D debajo el panel se ve a traves de la lente: al doble, como la pastilla */
  const auto hub = ajuste::leerEstadoHub();
  const auto ses = hub.find("sesion");
  const bool haySesion = ses != hub.end() && !ses->second.empty() && ses->second != "ninguna";
  const bool grande = haySesion || forzarGrande;
  const std::vector<prefs::Juego> juegos = prefs::juegosSteam();
  std::vector<std::string> ids;
  Hud::State &st = ov.hud().state();
  st.pestana = enJuegos ? 1 : 0;
  st.version = std::string("OdysseyVK ") + ODYSSEYVK_VERSION;
  auto rellenar = [&] { prefs::llenar(st, prefs::leer(), juegos, ids); };
  /* al abrirse, una mirada a GitHub (v0.99): asi la pestana Programa y el boton junto al aspa estan
     al dia sin esperar a la mirada del servicio. Es un curl aparte; si no hay red, no pasa nada. */
  if (prefs::leer().avisarActualizaciones) prefs::comprobarActualizacion(nullptr);
  rellenar();
  ov.showPanel(grande);
  if (!ov.askVisible()) {
    std::fprintf(stderr, "[ajustes] el compositor no puso el panel: %s\n", ov.lastError().c_str());
    ::unlink(rutaPid.c_str());
    return 1;
  }
  /* el teclado entero para el panel mientras esta abierto (v1.09): el juego o el video de debajo no
     reciben teclas, Esc lo cierra y los atajos se capturan aqui */
  ov.tomarTeclado(true);
  /* la entrada en la barra de tareas mientras el panel esta abierto (v1.04), si no se apago */
  if (prefs::leer().enBarraTareas) ov.ponEntradaBarra(idioma::tr("OdysseyVK — Ajustes"));
  std::printf("[ajustes] panel puesto en %s (%s) · %d juegos de Steam\n", ov.outputName().c_str(),
              grande ? "al doble: hay una sesion 3D" : "tamano de escritorio", (int)juegos.size());
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
  int idiomaDelAtlas = idioma::actual();
  while (!g_cierraPanel && !ov.conexionCaida() && !ov.entradaCerrada()) {
    ov.waitAndDispatch(100);
    Overlay::Golpe g;
    bool repintar = false;
    while (ov.takeGolpe(g)) {
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
      /* la lista de idiomas se cierra con cualquier clic que no sea en ella o en su boton */
      const bool clicIdioma = g.tipo == (int)Hud::HitPanelIdioma || g.tipo == (int)Hud::HitPanelIdiomaOpcion;
      if (st.menuIdioma && !clicIdioma) {
        st.menuIdioma = false;
        repintar = true;
      }
      if (g.tipo == (int)Hud::HitPanelCerrar) {
        g_cierraPanel = 1;
      } else if (g.tipo == (int)Hud::HitPanelIdioma) {
        st.menuIdioma = !st.menuIdioma;
        repintar = true;
      } else if (g.tipo == (int)Hud::HitPanelIdiomaOpcion) {
        st.menuIdioma = false;
        std::string aviso;
        bool cerrar = false;
        if (prefs::aplicar("idioma", g.sub, 0.0f, true, juegos, &aviso, &cerrar)) st.pie = aviso;
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
      } else if (g.tipo == (int)Hud::HitPanelPestana) {
        st.pestana = (g.sub >= 0 && g.sub <= 4) ? g.sub : 0;
        st.corrida = 0.0f;
        st.pie.clear();
        rellenar();
        repintar = true;
      } else if (g.tipo == (int)Hud::HitPanelItem && g.item >= 0 && (size_t)g.item < ids.size()) {
        std::string aviso;
        bool cerrar = false;
        const std::string id = ids[(size_t)g.item];
        const bool cambio = prefs::aplicar(id, g.sub, g.frac, g.soltado, juegos, &aviso, &cerrar);
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
    /* el teclado: con un atajo esperando, la combinacion va a el; si no, Esc cierra el panel (y las
       demas teclas se quedan aqui: el juego de debajo no las recibe) */
    {
      std::string combo;
      while (ov.takeTecla(combo)) {
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
          if (st.menuIdioma) {
            st.menuIdioma = false;
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
    if (st.pestana == 2 && reloj() - miradaRend >= 1.0) {
      miradaRend = reloj();
      rellenar();
      repintar = true;
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
        if (prefs::redAbiertaPresente()) {
          /* bajada y entera: el visor que haya abierto la carga ya (orden `cargar_red`), el servicio
             la ve en su proxima mirada, y este panel se cierra solo (v0.94) */
          std::string e;
          ajuste::escribir(std::map<std::string, double>(), std::string(), {{"cargar_red", "si"}}, &e);
          st.pie = idioma::tr("red descargada: el 3D de vídeo normal ya puede calcular la profundidad");
          cerrarEn = reloj() + 2.5;
        }
      }
    }
    if (repintar) ov.repaint();
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
  ov.hideAsk();
  ::unlink(rutaPid.c_str());
  return 0;
}

static int cmdCapa(int argc, char **argv) {
  const std::string uno = argc > 0 ? strLower(argv[0]) : "alternar";
  if (uno == "estado" || uno == "status" || uno == "info") {
    const std::map<std::string, std::string> est = ajuste::leerEstado();
    if (est.empty()) {
      std::printf("no hay visor abierto: %s esta vacio o no existe\n", ajuste::rutaEstado().c_str());
      return 1;
    }
    for (const char *k : {"capa", "trama", "facetracking"}) {
      const auto it = est.find(k);
      if (it != est.end()) std::printf("  %-12s %s\n", k, it->second.c_str());
    }
    std::printf("  (se oculta y se muestra con Ctrl+Shift+F1 si el visor tiene el teclado, o con este"
                " mismo comando)\n");
    return 0;
  }
  std::string valor;
  if (uno == "ocultar" || uno == "quitar" || uno == "0" || uno == "no" || uno == "off") valor = "ocultar";
  else if (uno == "mostrar" || uno == "poner" || uno == "1" || uno == "si" || uno == "on") valor = "mostrar";
  else if (uno == "alternar" || uno == "toggle") valor = "alternar";
  else {
    std::printf("uso: odysseyvk ctl capa [ocultar|mostrar|alternar|estado]\n");
    return 2;
  }
  std::string err;
  if (!ajuste::escribir(std::map<std::string, double>(), std::string(), {{"capa", valor}}, &err)) {
    std::fprintf(stderr, "%s\n", err.c_str());
    return 1;
  }
  std::printf("orden enviada al visor: pastilla %s (la trama sigue tejiendose; con el viso enfocado es"
              " la tecla Ctrl+Shift+F1)\n", valor.c_str());
  return 0;
}

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
  hud.state().controlOjo = true;
  hud.state().rastreoVivo = true;
  hud.state().hud = 3;
  hud.state().legado = o.legado;   /* `--legado` en la prueba: el deslizador de la izquierda cambia de nombre */
  /* la casilla de «no volver a preguntar» de la pastilla: a la vista y marcada, que es el caso ancho.
     ODYSSEYVK_HUD_RECORDAR=0 la quita (visor lanzado a mano) y =2 la deja a la vista sin marcar. */
  const char *rec = std::getenv("ODYSSEYVK_HUD_RECORDAR");
  hud.state().recordarVisible = !(rec && rec[0] == '0');
  hud.state().recordar = !(rec && rec[0] == '2');
  hud.state().remember = rec && rec[0] == '1';
  /* los botones de icono: ODYSSEYVK_HUD_MODO=0 los quita, 1 = video normal puesto, 2 = dividido,
     3 = dividido suavizado */
  const char *modo = std::getenv("ODYSSEYVK_HUD_MODO");
  hud.state().modoVisible = !(modo && modo[0] == '0');
  hud.state().modo3d = (modo && modo[0] >= '2' && modo[0] <= '3') ? modo[0] - '0' : 1;
  if (const char *linea = std::getenv("ODYSSEYVK_HUD_LINEA")) hud.setInfo(linea);
  if (const char *cuerpo = std::getenv("ODYSSEYVK_HUD_CUERPO")) hud.setAskText("", cuerpo);
  /* ODYSSEYVK_HUD_PANEL=0|1: el panel de ajustes en vez de la tarjeta, en esa pestana (0 = Ajustes,
     1 = Juegos), con lo que hay de verdad en serve.conf y en la biblioteca de Steam */
  hud.state().engranaje = true;
  if (const char *pn = std::getenv("ODYSSEYVK_HUD_PANEL")) {
    hud.state().ask = false;
    hud.state().panel = true;
    hud.state().pestana = std::max(0, std::min(4, std::atoi(pn)));
    std::vector<std::string> ids;
    /* ODYSSEYVK_HUD_CAPTURA=3d (v1.09): ese atajo esperando su combinacion nueva */
    std::string pieCaptura;
    if (const char *cap = std::getenv("ODYSSEYVK_HUD_CAPTURA")) {
      bool cerrar = false;
      prefs::aplicar(std::string("atajo:") + cap, 0, 0.0f, true, {}, &pieCaptura, &cerrar);
    }
    prefs::llenar(hud.state(), prefs::leer(), prefs::juegosSteam(), ids);
    hud.state().pie = pieCaptura.empty() ? std::string(idioma::tr("se aplica desde el siguiente vídeo")) : pieCaptura;
    hud.state().version = std::string("OdysseyVK ") + ODYSSEYVK_VERSION;
    if (const char *co = std::getenv("ODYSSEYVK_HUD_CORRIDA")) hud.state().corrida = (float)std::atof(co);
    /* ODYSSEYVK_HUD_MENU_IDIOMA=1 (v1.07): con la lista de idiomas abierta */
    if (std::getenv("ODYSSEYVK_HUD_MENU_IDIOMA")) hud.state().menuIdioma = true;
  }
  /* la pastilla con los atajos de serve.conf (v1.09), como la pone el visor */
  hud.state().teclas = atajos::renglonPastilla(o.legado);
  {
  }
  /* ODYSSEYVK_HUD_ACTUALIZACION=0.99 (taller, v0.95): la tarjeta de la orilla avisando de esa version */
  if (const char *av = std::getenv("ODYSSEYVK_HUD_ACTUALIZACION")) {
    hud.state().ask = true;
    hud.state().panel = false;
    hud.state().askActualizacion = av;
    hud.state().version = std::string("OdysseyVK ") + ODYSSEYVK_VERSION;
  }
  /* ODYSSEYVK_HUD_AVISO=1 (taller, v1.00): la tarjeta de aviso, con el texto del detector de caidas */
  if (std::getenv("ODYSSEYVK_HUD_AVISO")) {
    hud.state().ask = true;
    hud.state().panel = false;
    hud.state().askAviso = idioma::trf("El juego se cerró a los {} s de entrar en 3D", {"48"});
    hud.state().askAvisoLineas = {idioma::tr("Si se cayó, ábrelo en ventana sin bordes (borderless) y vuelve a probar."),
                                  idioma::tr("A pantalla completa exclusiva choca con el visor al cambiar de modo."),
                                  idioma::tr("Para dejarlo en 2D, cambia su interruptor en la pestaña Juegos.")};
  }
  /* ODYSSEYVK_HUD_RATON=x,y deja el puntero quieto en ese punto: se ve el realce de lo que tenga debajo */
  if (const char *raton = std::getenv("ODYSSEYVK_HUD_RATON")) {
    float rx = -1.0f, ry = -1.0f;
    if (std::sscanf(raton, "%f,%f", &rx, &ry) == 2) {
      hud.begin(W, H);   /* la escala sale del primer trazado */
      hud.setMouse(rx, ry);
    }
  }
  hud.begin(W, H);
  std::printf("[interfaz] ventana %dx%d · atlas %dx%d · texto %s · %d vertices\n", W, H, hud.atlasW(), hud.atlasH(),
              hud.hasText() ? "si" : "no", hud.vertexCount());
  /* ODYSSEYVK_HUD_PNG=ruta deja la interfaz pintada en un archivo (fondo transparente), con el mismo
     pintado de CPU que usa la tarjeta de la orilla: sirve para mirarla sin el panel delante. */
  if (const char *png = std::getenv("ODYSSEYVK_HUD_PNG")) {
    std::vector<float> acum;
    const auto tPinta = std::chrono::steady_clock::now();
    hud.pintarCpu(0.0f, 0.0f, W, H, acum);
    std::printf("  pintado en CPU de %dx%d: %.1f ms\n", W, H,
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tPinta).count());
    std::vector<uint8_t> rgba((size_t)W * (size_t)H * 4u);
    for (size_t p = 0; p < (size_t)W * (size_t)H; p++) {
      const float A = acum[p * 4u + 3u];
      for (int c = 0; c < 3; c++)
        rgba[p * 4u + (size_t)c] =
            (uint8_t)std::lround(255.0f * std::max(0.0f, std::min(1.0f, A > 0.0001f ? acum[p * 4u + (size_t)c] / A : 0.0f)));
      rgba[p * 4u + 3u] = (uint8_t)std::lround(255.0f * std::max(0.0f, std::min(1.0f, A)));
    }
    std::printf("  volcado a %s: %s\n", png, glPngSave(png, W, H, rgba.data()) ? "hecho" : "NO SE PUDO");
  }
  if (hud.state().panel) {
    /* el panel de ajustes: cuantas celdas de raton tiene cada cosa que se pica, y con que senas */
    struct Cuenta { int celdas = 0, subs = 0; float fmin = 9.0f, fmax = -9.0f; };
    std::vector<Cuenta> porFila(hud.state().items.size());
    int cerrar = 0, pestanas[4] = {0, 0, 0, 0}, vacio = 0, fuera = 0;
    for (int y = 0; y < H; y += 2)
      for (int x = 0; x < W; x += 2) {
        int item = -1, sub = 0;
        float fr = 0.0f;
        const Hud::Hit g = hud.hitDetalle((float)x, (float)y, &item, &sub, &fr);
        if (g == Hud::HitNone) fuera++;
        else if (g == Hud::HitPanel) vacio++;
        else if (g == Hud::HitPanelCerrar) cerrar++;
        else if (g == Hud::HitPanelPestana) pestanas[sub & 3]++;
        else if (g == Hud::HitPanelItem && item >= 0 && (size_t)item < porFila.size()) {
          Cuenta &c = porFila[(size_t)item];
          c.celdas++;
          c.subs |= 1 << (sub & 7);
          c.fmin = std::min(c.fmin, fr), c.fmax = std::max(c.fmax, fr);
        }
      }
    float bx = 0, by = 0, bw = 0, bh = 0;
    hud.panelBounds(&bx, &by, &bw, &bh);
    std::printf("  panel de ajustes: %.0f x %.0f px en %.0f,%.0f (escala %.2f) · aspa %d celdas · pestanas %d / %d\n",
                (double)bw, (double)bh, (double)bx, (double)by, (double)hud.scale(), cerrar, pestanas[0], pestanas[1]);
    static const char *kTipo[] = {"seccion", "interruptor", "opciones", "deslizador", "fila", "juego", "nota"};
    for (size_t i = 0; i < porFila.size(); i++) {
      const Hud::ItemPanel &it = hud.state().items[i];
      const Cuenta &c = porFila[i];
      std::printf("    %-11s %5d celdas", kTipo[(int)it.tipo], c.celdas);
      if (it.tipo == Hud::ItemPanel::Opciones) std::printf(" · opciones vistas %d%d%d", c.subs & 1, (c.subs >> 1) & 1, (c.subs >> 2) & 1);
      if (it.tipo == Hud::ItemPanel::Deslizador && c.celdas) std::printf(" · recorrido %.2f..%.2f", (double)c.fmin, (double)c.fmax);
      if (it.tipo == Hud::ItemPanel::Fila) std::printf(" · aspa %s", (c.subs & 2) ? "si" : "no");
      std::printf("  %s\n", it.rotulo.c_str());
    }
    hud.correr(10000.0f);
    hud.begin(W, H);
    std::printf("    la rueda: la lista corre hasta %.0f puntos\n", (double)hud.state().corrida);
    return 0;
  }
  /* la lista de golpes de raton: nada, interruptor, dos deslizadores, (el boton de la red, que ya
     no existe y tiene que dar cero), las teselas de la tarjeta, su casilla, el aspa, el "?" y la
     casilla de la pastilla */
  static const char *nom[] = {"nada",                "interruptor 3D",      "deslizador profundidad",
                              "deslizador salida",   "boton red (quitado)", "tesela video dividido",
                              "tesela arriba/abajo", "tesela plana",        "tesela video normal",
                              "casilla de la tarjeta", "cerrar la tarjeta", "ayuda de la tarjeta",
                              "casilla de la pastilla", "boton video normal", "boton video dividido",
                              "boton dividido suavizado"};
  const int casos = 15;
  int cuenta[16] = {0}, px0[16] = {0}, py0[16] = {0};
  for (int y = 0; y < H; y += 2)
    for (int x = 0; x < W; x += 2) {
      int h = (int)hud.hit((float)x, (float)y);
      if (h < 0 || h > casos) continue;
      if (!cuenta[h]) {
        px0[h] = x;
        py0[h] = y;
      }
      cuenta[h]++;
    }
  for (int h = 1; h <= casos; h++)
    std::printf("  %-24s %6d celdas de raton · primer golpe (%d,%d)\n", nom[h], cuenta[h], px0[h], py0[h]);

  /* Rotulos, con la caja real donde se dibujan. Si la letra mide mas que su caja se sale del
     recuadro. La pastilla: el interruptor mide 100 (quedan 60 para la palabra), cada deslizador 140
     (rotulo a la izquierda y numero a la derecha) y el renglon de teclas se achica solo si no cabe.
     La tarjeta: 500 de ancho con 26 de borde; cada tesela mide 217. */
  struct Medida {
    const char *texto;
    float px, caja;
  };
  const float tesela = (500.0f - 52.0f - 14.0f) * 0.5f - 16.0f;
  const Medida med[] = {{"ON", 15.0f, 60.0f},
                        {"OFF", 15.0f, 60.0f},
                        {"Profundidad  100", 12.0f, 140.0f},
                        {"Plano de apoyo  100", 12.0f, 140.0f},
                        {"Salida  -50", 12.0f, 140.0f},
                        {"Ctrl+Shift+  1 el 3D · 3/4 profundidad · 5/6 salida · F1 oculta · Esc sale", 10.5f,
                         hud.panelW() - 32.0f},
                        {"¿Qué tipo de vídeo estás viendo?", 18.0f, 500.0f - 26.0f - 92.0f},
                        {"Elige la opción correcta para la conversión 3D.", 13.5f, 448.0f},
                        {"Vídeo normal: una sola imagen; se convierte a 3D.", 12.5f, 448.0f},
                        {"Vídeo dividido: trae dos imágenes, una al lado de la otra.", 12.5f, 448.0f},
                        {"Vídeo normal", 15.0f, tesela},
                        {"Vídeo dividido", 15.0f, tesela},
                        {"una sola imagen", 11.5f, tesela},
                        {"dos imágenes lado a lado", 11.5f, tesela},
                        {"Guardar el tipo de vídeo y no volver a preguntar", 13.0f, 448.0f - 30.0f}};
  for (const Medida &m : med) {
    const float w = hud.mideTexto(m.texto, m.px);
    std::printf("  letra  %4.0fpx %6.1f px de %5.0f  %s  %s\n", (double)m.px, (double)w, (double)m.caja,
                w <= m.caja ? "cabe    " : "SE SALE ", m.texto);
  }

  /* de cuanto salio la pastilla: 462 x 76 sin la casilla y unos 650 con ella (antes de v0.78 eran
     824 x 134). Si la ventana se queda angosta, la pastilla se encoge con ella. */
  std::printf("  pastilla dibujada: %.0f x %.0f px en ventana de %dx%d (%s, %s)\n", (double)hud.panelW(),
              (double)hud.panelH(), W, H, o.legado ? "modo de legado" : "modo completo",
              hud.state().recordarVisible ? "con la casilla" : "sin la casilla");
  {
    float ax = 0, ay = 0, aw = 0, ah = 0;
    if (hud.askBounds(&ax, &ay, &aw, &ah))
      std::printf("  tarjeta dibujada: %.0f x %.0f px reales en %.0f,%.0f (escala %.2f)\n", (double)aw, (double)ah,
                  (double)ax, (double)ay, (double)hud.scale());
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
  std::printf("  interruptor ON/OFF: tramado %s -> %s\n", w0 ? "activo" : "apagado",
              hud.state().weaveOn ? "activo" : "apagado");
  const bool o0 = hud.state().overlayOn;
  hud.toggleOverlay();   /* la tecla Ctrl+Shift+F1 y `ctl capa` caen aqui dentro */
  std::printf("  pastilla con Ctrl+Shift+F1: superposicion %s -> %s\n", o0 ? "visible" : "oculta",
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
    std::printf("    si antes estaba: su USB se quedo colgado (dmesg: \"device descriptor read/64,\n");
    std::printf("    error -110\") y vuelve solo apagando y encendiendo el monitor\n");
    std::string alterna;
    if (pickCameraDevice(alterna, nullptr) && !alterna.empty()) {
      std::printf("    hay otra camara (%s): el rastreo puede ir con ella por el tamano de la cara\n",
                  alterna.c_str());
      std::printf("    (aproximado: la distancia sale del ancho de la cara; se ajusta con --track-cara)\n");
    }
    problems++;
  } else {
    for (const std::string &c : cams)
      std::printf("  %s  (accesible: %s)\n", c.c_str(), access(c.c_str(), R_OK | W_OK) == 0 ? "si" : "NO");
    std::string nota;
    Camera::keepAwake(cams.front(), &nota);
    std::printf("  energia    : %s  (%s)\n", Camera::powerState(cams.front()).c_str(), nota.c_str());
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
      if (st.scale != 1.0 || !Panel::isLandscape(st.rotation)) { std::printf("  ! el panel no esta a pixel a pixel: usa `odysseyvk ctl panel apply`\n"); }
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
  std::string nuestra;            /* la otra red (.onnx), la nativa, la del boton 2 del visor */
  std::string modo = "ask";   /* ask = preguntar; sbs/tb/flat/conv = arrancar sin preguntar */
  int red = 0;                /* GPU donde vive la red que convierte video normal a 3D */
  int cada = 3;               /* cada cuantos cuadros se calcula la profundidad */
  bool rastreo = false;       /* pasar --track al visor: la camara IR del monitor sigue los ojos */
  float suavizado = 0.0f;     /* 0 = el valor de fabrica del visor (0,35); menor = responde antes */
  std::string camara;         /* dispositivo concreto; vacio = elegir el mejor */
  std::string mono = "auto";  /* si = webcam normal por la cara; no = la camara del monitor; auto = mirar cual es */
  double campo = 65.0;        /* campo horizontal de la webcam, en grados */
  double cara = 140.0;        /* ancho de cara de oreja a oreja: el numero de Samsung (tracker-ft_
                                   user.ini, FaceWidth_mm=140). Ojo: el hub pasa este valor al visor, asi que
                                   mientras estuviera en 150 la correccion del visor no llegaba a ninguna sesion. */
  std::string texto = "auto";  /* rotulos apoyados en la pantalla: si / no / auto (auto = si) */
  /* Modo de legado: el video ya llega en dos vistas, asi que la trama trabaja con los numeros de
     fabrica, no entra el conversor que saca dos vistas de una y la pastilla sale sencilla (interruptor,
     salida 3D y plano de apoyo). Se enciende y se apaga desde la pestana General de la ventana de
     ajustes o con `ctl legado`, y queda escrito en serve.conf para la siguiente sesion. */
  bool legado = false;
  /* Ajuste de la trama contra la lente de ESTA unidad: son los mismos numeros que se mueven en
     marcha con las teclas, pero aqui quedan guardados y se pasan a todas las sesiones. */
  double fase = 0.0;        /* desfase de la trama, en ciclos (flechas izquierda y derecha) */
  double paso = 1.0;        /* escala del paso de la lente (RePag y SavPg) */
  double filtro = -1.0;     /* pendiente del filtro (coma y punto); -1 = la de fabrica, que es 10 */
  std::string calib;        /* carpeta con los mapas de correccion de ESTE panel */
  std::string volcado;      /* si se pone, el visor guarda -trama / -profundidad / -origen ahi */
  /* Tres numeros para medir sin recompilar (el visor los lee de variables de entorno). Sirven para
     separar las causas de una franja: `relieve = 0` deja la imagen PLANA del todo, que no se
     consigue con Ctrl+Shift+5 porque en 0 de "3D Salida" quedan 4 px de separacion entre ojos. */
  double relieve = -1.0;    /* escala de la ganancia del releve; 0 = sin releve ninguno */
  std::string saliencia;    /* si / no: elegir el plano de apoyo con la saliencia del Hub (v0.65) */
  std::string cantos;       /* `viejo` = el reparto del mapa por ojo de antes de la v0.89; vacio = cantos finos */
  double pivote = -1.0;     /* 0..0,9: cuanto se retrasa el plano de la pantalla respecto al sujeto (v0.66) */
  double paralaje = 999.0;  /* ganancia del paralaje de movimiento, -2..2; 999 = sin poner (apagado) (v0.67) */
  double detalle = -1.0;    /* fuerza del realce de detalle en el sujeto, 0..3; -1 = la de fabrica (0,4) (v0.68) */
  double mapasSeg = -1.0;   /* tope de mapas de profundidad por segundo, 2..60; -1 = el de fabrica (6) (v0.70) */
  std::string hdrEn3d;      /* si = no apagar el HDR del panel al entrar en 3D (v0.71) */
  std::string rastreoFino;  /* no = el rastreo de antes de la v0.74 (los ojos de YuNet restados) */
  std::string barra2d;      /* no = en el video dividido no tejer en 2D las filas de la barra del reproductor (v0.81);
                               entera = tejerlas tal cual vienen, sin la limpieza de la v0.90 */
  std::string actualizaciones;  /* no = no avisar con la tarjeta cuando hay una version nueva en GitHub (v0.95) */
  std::string actualizarAuto;   /* si = con una version nueva, `odysseyvk actualizar` sin preguntar (v0.95) */
  bool barraTareas = true;   /* el visor en la barra de tareas (v1.02) */
  bool bandeja = true;       /* el icono en la bandeja del sistema (v1.03) */
  double actualizacionesCada = 6.0;   /* horas entre miradas a GitHub (v0.99: `actualizaciones_cada`, 0,25..168) */
  std::string atajosGlobales; /* no = no registrar los atajos del hub como globales del escritorio (v0.75) */
  /* la rama experimental (pestana Experimental del panel): cada prueba se pasa al visor por entorno */
  std::string expRed;             /* la entrada de la red: cuadrado (de fabrica) | 518 | 770 | 1022 (16:9, lado largo) */
  std::string expMapa;            /* puntos a lo ancho del mapa: 518 (de fabrica) | 770 | 1022 */
  std::string expTope;            /* si = tope de arrastre en el suavizado en el tiempo */
  double expAnticipacion = -1.0;  /* ms de anticipacion de la cabeza; < 0 = no puesto (0) */
  std::string expPrioridad;       /* normal | alta | tiempo_real: la cola del visor en la tarjeta */
  std::string escala3d;       /* 1 = pasar el panel a escala 1 al entrar en 3D, como antes de la v0.76 */
  std::string vrrEn3d;      /* si = dejar la sincronizacion adaptativa durante el 3D; de fabrica se apaga (v1.11) */
  double hz = -1.0;         /* frecuencia del panel en 3D; -1 = la de fabrica (120) (v0.71) */
  double relieveLocal = -1.0; /* alfa del relieve local; -1 = el de fabrica (1,0), 0 = apagado (v0.66) */
  double focoIir = -1.0;    /* cuanto se mueve el plano de apoyo por estimacion; -1 = el de fabrica (0,06) */
  double temblor = -1.0;    /* pixeles de temblor del desplazamiento; -1 = el de fabrica (0,25) */
  double crosstalkBase = -1.0;   /* compensacion pareja de la diafonia, 0..0,25; -1 = la de fabrica (0,0129) (v0.83) */
  /* Los videos divididos se ven SUAVIZADOS (una de sus dos imagenes y el relieve del conversor) salvo
     que aqui se diga que no (v0.84). Es el unico sitio donde se quita: `dividido_suavizado = no`, o
     el interruptor de los ajustes del programa. (`suavizado` a secas ya era otra cosa: el filtro del
     rastreo de la cabeza.) */
  bool divididoSuave = true;
  bool divididoAuto = false;   /* dividido_suavizado = auto: solo mientras el video se salga (v0.88) */
  double canto = -1.0;      /* ancho del puente del canto; -1 = el de fabrica (0,004) */
  double faseTemblor = -1.0;/* temblor de la fase de la trama, en ciclos; -1 = apagado (0) */
  double rampaX = 999.0;    /* rampa de fase en x, ciclos por pixel de panel; 999 = sin ajustar */
  double rampaY = 999.0;    /* lo mismo en y (hacia abajo en la pantalla) */
  /* Capas por profundidad: `fondo` cuanto se estira lo que queda detras del plano de apoyo y
     `frente` lo que sale delante. -1 = sin ajustar (1,0 en los dos, como siempre). Mueven lo
     mismo que las teclas j/k y u/i, pero dejado por escrito para comparar mananas enteras. */
  double ganFondo = -1.0;
  double ganFrente = -1.0;
  /* empuje por capa, en puntos de panel; 999 = no puesto (0 si vale: es el neutro) */
  double empFondo = 999.0;
  double empFrente = 999.0;
  /* Dos planos: fuerza con la que el mapa de profundidad se aplasta hacia el fondo y los primeros
     planos del cuadro. -1 = sin ajustar (apagado, el relieve continuo de siempre); de 0,0 a 1,0.
     Es lo mismo que sube y baja la tecla `g`, pero dejado por escrito. */
  double planos = -1.0;
  /* Muestras intermedias entre las dos laminas. -1 = sin ajustar (0 = la rampa continua de siempre);
     de 0 a 3. Con 1 el centro del cuadro tiene su propia meseta en vez de un degradado. */
  int muestras = -1;
  /* Cuanto tiene que decir el navegador "Parado" para cerrar la sesion (segundos). 0 apaga el
     vigia por MPRIS. Hace falta porque al salir de un video de YouTube la pagina del navegador
     suele seguir a pantalla completa: por la ventana no se entera uno de que el video se fue, y
     la trama se queda tejida sobre el navegador. */
  int paradoSeg = 4;
  /* Al irse el video, la ventana del navegador NO se queda a pantalla completa. Medido el 24/11 a
     las 20:53: cerramos la sesion porque la ventana del video salio de pantalla completa y UN segundo
     despues Firefox se la volvio a poner el solo (no eramos nosotros: nuestro aviso de "se la
     devolvemos" solo habia salido al arrancar). Se apaga con `devolver_fullscreen = no`. */
  bool devolverFull = true;
  std::map<std::string, std::string> porApp;   /* app_id -> modo, para no volver a preguntar */
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

/* "~" al principio de una ruta de modelo, para poder escribirlo a mano en la configuracion. */
static std::string expansionRuta(std::string r) {
  const char *h = std::getenv("HOME");
  const std::string casa = h ? h : "";
  if (!casa.empty() && (r == "~" || (r.size() > 1 && r.compare(0, 2, "~/") == 0)))
    r = casa + r.substr(1);
  return r;
}

/* ~/.config/odysseyvk/serve.conf: lineas clave=valor (ignore=, espera=, modo=, pantalla=,
   modelo= la red del boton 1, nuestra= la nativa del boton 2, rastreo= si/no, suavizado= 0,35,
   calib= carpeta de los mapas de correccion de este panel, fase= paso= filtro= ajuste de la trama). */
static void loadServeCfg(ServeCfg &cfg) {
  const char *home = std::getenv("HOME");
  if (!home) return;
  std::string txt;
  if (!readSmallFile(std::string(home) + "/.config/odysseyvk/serve.conf", txt)) return;
  std::string line;
  auto eat = [&](bool last) {
    std::string l = strTrim(line);
    line.clear();
    if (l.empty() || l[0] == '#') return;
    size_t eq = l.find('=');
    if (eq == std::string::npos) return;
    std::string k = strLower(strTrim(l.substr(0, eq))), v = strTrim(l.substr(eq + 1));
    /* un comentario al final de la linea («vrr_en_3d = no   # prueba») no es parte del valor. Sin esto
       el valor era «no   # prueba», que no es «no»: MEDIDO 26-nov, el VRR siguio puesto en el 3D. */
    for (size_t h = v.find('#'); h != std::string::npos; h = v.find('#', h + 1))
      if (h > 0 && (v[h - 1] == ' ' || v[h - 1] == '\t')) {
        v = strTrim(v.substr(0, h));
        break;
      }
    if (k == "ignore") splitList(v, cfg.ignore);
    else if (k == "espera") cfg.esperaSeg = std::max(5, std::atoi(v.c_str()));
    else if (k == "red") cfg.red = std::max(0, std::atoi(v.c_str()));
    else if (k == "cada") cfg.cada = std::max(1, std::atoi(v.c_str()));
    else if (k == "modo") cfg.modo = strLower(v);
    else if (k == "pantalla") cfg.pantalla = v;
    else if (k == "modelo") cfg.modelo = expansionRuta(v);
    else if (k == "nuestra") cfg.nuestra = expansionRuta(v);
    else if (k == "rastreo") cfg.rastreo = (v == "si" || v == "on" || v == "1" || v == "track");
    else if (k == "camara") cfg.camara = v;
    else if (k == "mono") {
      const std::string m = strLower(v);
      cfg.mono = (m == "si" || m == "no" || m == "auto") ? m : "auto";
    }
    else if (k == "texto") {
      const std::string t = strLower(v);
      cfg.texto = (t == "si" || t == "no" || t == "auto") ? t : "auto";
    }
    else if (k == "legado") {
      const std::string s = strLower(v);
      cfg.legado = (s == "si" || s == "on" || s == "1" || s == "encendido" || s == "puesto");
    }
    else if (k == "devolver_fullscreen") {
      const std::string s = strLower(v);
      cfg.devolverFull = !(s == "no" || s == "off" || s == "0" || s == "apagado");
    }
    else if (k == "campo") {
      std::string numero = v;
      for (char &c : numero)
        if (c == ',') c = '.';
      const double d = std::atof(numero.c_str());
      cfg.campo = (d >= 20.0 && d <= 170.0) ? d : 65.0;
    }
    else if (k == "cara") {
      std::string numero = v;
      for (char &c : numero)
        if (c == ',') c = '.';
      const double d = std::atof(numero.c_str());
      cfg.cara = (d >= 80.0 && d <= 260.0) ? d : 140.0;
    }
    else if (k == "suavizado") {
      std::string numero = v;
      for (char &c : numero)
        if (c == ',') c = '.';
      cfg.suavizado = (float)std::atof(numero.c_str());
    }
    else if (k == "calib") cfg.calib = expansionRuta(v);
    else if (k == "volcado") cfg.volcado = expansionRuta(v);
    else if (k == "saliencia") cfg.saliencia = v;
    else if (k == "cantos") cfg.cantos = v;
    else if (k == "hdr_en_3d") cfg.hdrEn3d = v;
    else if (k == "rastreo_fino") cfg.rastreoFino = v;
    else if (k == "barra_2d") cfg.barra2d = v;
    else if (k == "actualizaciones") cfg.actualizaciones = v;
    else if (k == "actualizar_auto") cfg.actualizarAuto = v;
    else if (k == "barra_tareas") cfg.barraTareas = !(strLower(v) == "no" || v == "0");
    else if (k == "bandeja") cfg.bandeja = !(strLower(v) == "no" || v == "0");
    else if (k == "actualizaciones_cada") {
      std::string numero = v;
      for (char &c : numero)
        if (c == ',') c = '.';
      const double d = std::atof(numero.c_str());
      cfg.actualizacionesCada = (d >= 0.25 && d <= 168.0) ? d : 6.0;
    }
    else if (k == "atajos_globales") cfg.atajosGlobales = v;
    else if (k == "exp_red") cfg.expRed = strLower(v);
    else if (k == "exp_mapa") cfg.expMapa = v;
    else if (k == "exp_tope_arrastre") cfg.expTope = strLower(v);
    else if (k == "exp_prioridad") cfg.expPrioridad = strLower(v);
    else if (k == "exp_anticipacion") {
      std::string t = v;
      for (char &c : t) if (c == ',') c = '.';
      cfg.expAnticipacion = std::max(0.0, std::min(60.0, std::atof(t.c_str())));
    }
    else if (k == "escala_3d") cfg.escala3d = v;
    else if (k == "vrr_en_3d") cfg.vrrEn3d = v;
    else if (k == "hz") {
      const double d = std::atof(v.c_str());
      cfg.hz = (d >= 50.0 && d <= 250.0) ? d : -1.0;
    }
    else if (k == "pivote" || k == "relieve_local" || k == "paralaje" || k == "detalle" ||
             k == "mapas_por_segundo") {
      std::string numero = v;
      for (char &c : numero)
        if (c == ',') c = '.';
      const double d = std::atof(numero.c_str());
      if (k == "pivote") cfg.pivote = (d >= 0.0 && d <= 0.9) ? d : -1.0;
      else if (k == "paralaje") cfg.paralaje = (d >= -2.0 && d <= 2.0) ? d : 999.0;
      else if (k == "detalle") cfg.detalle = (d >= 0.0 && d <= 3.0) ? d : -1.0;
      else if (k == "mapas_por_segundo") cfg.mapasSeg = (d >= 2.0 && d <= 60.0) ? d : -1.0;
      else cfg.relieveLocal = (d >= 0.0 && d <= 3.0) ? d : -1.0;
    }
    else if (k == "foco_iir") {
      std::string numero = v;
      for (char &c : numero)
        if (c == ',') c = '.';
      const double d = std::atof(numero.c_str());
      cfg.focoIir = (d > 0.0 && d <= 1.0) ? d : -1.0;
    }
    else if (k == "dividido_suavizado") {
      const std::string s = strLower(v);
      cfg.divididoAuto = (s == "auto" || s == "automatico");
      cfg.divididoSuave = !(s == "no" || s == "0" || s == "off" || s == "apagado" || s == "quitado");
    }
    else if (k == "crosstalk_base") {
      std::string numero = v;
      for (char &c : numero)
        if (c == ',') c = '.';
      const double d = std::atof(numero.c_str());
      cfg.crosstalkBase = (d >= 0.0 && d <= 0.25) ? d : -1.0;
    }
    else if (k == "fase" || k == "paso" || k == "filtro" ||
             k == "relieve" || k == "temblor" || k == "canto" || k == "fase_temblor" ||
             k == "rampa_x" || k == "rampa_y" || k == "fondo" || k == "frente" ||
             k == "empuje_fondo" || k == "empuje_frente" || k == "planos" || k == "muestras" ||
             k == "video_parado") {
      /* la trama se desalinea medio por ciento, asi que estos numeros van con decimales */
      std::string numero = v;
      for (char &c : numero)
        if (c == ',') c = '.';
      const double d = std::atof(numero.c_str());
      if (k == "fase") cfg.fase = d;
      else if (k == "paso") cfg.paso = (d > 0.5 && d < 2.0) ? d : 1.0;
      else if (k == "filtro") cfg.filtro = d;
      else if (k == "relieve") cfg.relieve = d;
      else if (k == "temblor") cfg.temblor = d;
      else if (k == "canto") cfg.canto = d;
      else if (k == "rampa_x") cfg.rampaX = (d > -0.05 && d < 0.05) ? d : 999.0;
      else if (k == "rampa_y") cfg.rampaY = (d > -0.05 && d < 0.05) ? d : 999.0;
      else if (k == "fondo") cfg.ganFondo = (d >= 0.2 && d <= 2.0) ? d : -1.0;
      else if (k == "frente") cfg.ganFrente = (d >= 0.2 && d <= 2.0) ? d : -1.0;
      /* empuje de cada capa, en puntos de panel (positivo = hacia fuera de la pantalla) */
      else if (k == "empuje_fondo") cfg.empFondo = (d >= -15.0 && d <= 15.0) ? d : 999.0;
      else if (k == "empuje_frente") cfg.empFrente = (d >= -15.0 && d <= 15.0) ? d : 999.0;
      /* dureza de los dos planos: 0 = apagado, 1 = laminas planas; fuera de ese rango no se usa */
      else if (k == "planos") cfg.planos = (d >= 0.0 && d <= 1.0) ? d : -1.0;
      /* muestras intermedias: 0 = rampa continua, 1..3 niveles extra entre las laminas */
      else if (k == "muestras") cfg.muestras = (d >= 0.0 && d <= 3.0) ? (int)d : -1;
      /* segundos que el navegador tiene que decir "Parado" para cerrar la sesion; 0 = apagado */
      else if (k == "video_parado") cfg.paradoSeg = (d >= 0.0 && d <= 600.0) ? (int)d : 4;
      else cfg.faseTemblor = d;
    }
    else if (k == "auto") {
      /* auto = mpv:sbs, haruna:tb  -> esas ventanas pasan directas, sin preguntar */
      std::vector<std::string> pares;
      splitList(v, pares);
      for (const std::string &par : pares) {
        size_t dd = par.find(':');
        if (dd == std::string::npos) continue;
        std::string app = strLower(strTrim(par.substr(0, dd))), md = strLower(strTrim(par.substr(dd + 1)));
        if (!app.empty() && (md == "sbs" || md == "tb" || md == "flat" || md == "conv" || md == "suave")) cfg.porApp[app] = md;
      }
    }
    (void)last;
  };
  for (char c : txt) {
    if (c == '\n') eat(false);
    else line += c;
  }
  eat(true);
  /* La lista `ignore` del archivo trae el nombre viejo (odyssey3d) y desde v0.57 la app se llama
     odysseyvk: si solo se mira lo escrito, nuestro propio visor quedaria sin ignorar y el
     vigilante se lanzaria un 3D encima de si mismo. Los dos nombres van siempre. */
  for (const char *propia : {"odysseyvk", "odyssey3d"}) {
    bool puesta = false;
    for (const std::string &ig : cfg.ignore)
      if (ig == propia) puesta = true;
    if (!puesta) cfg.ignore.push_back(propia);
  }
}

static std::string selfPath() {
  char buf[4096];
  ssize_t n = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
  if (n <= 0) return "odysseyvk";
  buf[n] = 0;
  return buf;
}

static bool ignoredApp(const WindowInfo &w, const std::vector<std::string> &list) {
  std::string app = strLower(w.appId), title = strLower(w.title);
  /* El titulo de nuestras ventanas lo ponemos nosotros: "OdysseyVK" desde v0.57 y el nombre largo
     antes. Se comprueban los dos para no quedarse ciego con el nombre viejo. */
  if (title.find("odysseyvk") != std::string::npos || title.find("odyssey 3d") != std::string::npos)
    return true;
  for (const std::string &ig : list) {
    if (ig.empty()) continue;
    if (app == ig || app.find(ig) != std::string::npos) return true;
  }
  return false;
}

static std::string g_modoVisor;   /* el modo del ultimo visor lanzado (sbs, conv...), para reabrirlo tras actualizar (v0.95) */
static pid_t spawnViewer(const std::string &exe, const std::string &uuid, const std::string &modoPedido, int red = 0,
                        int cada = 3, const std::string &modelo = std::string(),
                        const std::string &pantalla = std::string(),
                        const std::string &alterno = std::string(), bool rastreo = false,
                        float suavizado = 0.0f, ServeCfg *pila = nullptr, bool escritorio = false,
                        const std::string &recordar = std::string(), bool recordado = false) {
  /* El modo de legado se cambia desde la ventana de ajustes sin reiniciar el servicio, y el
     vigilante leia serve.conf una sola vez al arrancar: lo volvemos a mirar justo antes de lanzar el
     visor, para que el modo que se ve marcado en la ventana sea el que sale en pantalla. Solo se
     refresca este valor; los demas siguen con los de arranque (y con las banderas de la linea de
     orden), que es lo que dice el README: esos numeros piden reiniciar el servicio. */
  g_modoVisor = modoPedido;
  if (pila) {
    const std::map<std::string, std::string> conf = ajuste::leerServeConf();
    if (const auto it = conf.find("legado"); it != conf.end()) {
      const std::string v = strLower(it->second);
      pila->legado = (v == "si" || v == "on" || v == "1" || v == "encendido" || v == "puesto");
    }
    /* y lo mismo con el suavizado de los videos divididos, que tambien se cambia desde los ajustes */
    if (const auto it = conf.find("dividido_suavizado"); it != conf.end()) {
      const std::string v = strLower(it->second);
      pila->divididoAuto = (v == "auto" || v == "automatico");
      pila->divididoSuave = !(v == "no" || v == "0" || v == "off" || v == "apagado" || v == "quitado");
    } else {
      pila->divididoSuave = true;
      pila->divididoAuto = false;
    }
  }
  /* El modo que de verdad se lanza. «Dividido» (`sbs`, y el `suave` que guardo la 0.83) es UNA
     eleccion; que salga suavizado o tal cual lo dicen los ajustes del programa. */
  const std::string modo = (pila && (modoPedido == "sbs" || modoPedido == "suave"))
                               ? std::string(pila->divididoSuave && !pila->divididoAuto ? "suave" : "sbs")
                               : modoPedido;
  pid_t pid = ::fork();
  if (pid != 0) return pid;
  /* `escritorio = true` es el overlay FORZADO sobre el escritorio del monitor: no hay ventana de
     video que capturar (capturar la salida del monitor se morderia la cola: tejeria su propia trama),
     asi que el visor teje su patron de trama de siempre. Es el mismo camino de `odysseyvk test`:
     panel a 3D, lente, pastilla y rastreo incluidos, solo que sin imagen de video detras. */
  std::vector<std::string> args =
      escritorio ? std::vector<std::string>{exe, "test", "--siempre"}
                 : std::vector<std::string>{exe, "capture3d", "--uuid", uuid};
  if (!pantalla.empty()) { args.push_back("--output"); args.push_back(pantalla); }
  if (modo == "sbs") args.push_back("--sbs");
  if (modo == "tb") args.push_back("--tb");
  if (modo == "flat") args.push_back("--2d");
  /* la casilla de «no volver a preguntar» dentro del video: `aplicacion:modo`, y si ya esta apuntada */
  if (!recordar.empty()) {
    args.push_back("--recordar");
    args.push_back(recordar);
    if (recordado) args.push_back("--recordado");
  }
  /* el modo de legado: el visor arma la trama con los numeros de fabrica, no abre el conversor que
     saca dos vistas de una y pinta la pastilla sencilla. Aqui arriba no se pasan a proposito ni la
     fase, ni el paso, ni el filtro, ni los planos: ahi la trama lleva lo de fabrica. */
  const bool legadoPuesto = pila && pila->legado;
  if (legadoPuesto) {
    args.push_back("--legado");
    if (modo == "conv" || modo == "suave")
      std::fprintf(stderr, "[serve] modo de legado: `modo = conv` no va ahi (aqui el video ya llega en"
                           " dos vistas); se pregunta el formato al arrancar\n");
    else
      std::fprintf(stderr, "[serve] modo de legado puesto: trama de fabrica, sin conversor y pastilla sencilla\n");
  }
  if (pila && !legadoPuesto) {
    char valor[32];
    /* el ajuste fino de la trama contra la lente de esta unidad, si esta en serve.conf.
       En modo de legado esto no se pasa: alli la trama lleva los numeros de fabrica. */
    if (pila->fase != 0.0) {
      std::snprintf(valor, sizeof valor, "%.6f", pila->fase);
      args.push_back("--align");
      args.push_back(valor);
    }
    if (pila->paso != 1.0) {
      std::snprintf(valor, sizeof valor, "%.6f", pila->paso);
      args.push_back("--pitch");
      args.push_back(valor);
    }
    if (pila->filtro >= 0.0) {
      std::snprintf(valor, sizeof valor, "%.3f", pila->filtro);
      args.push_back("--filtro");
      args.push_back(valor);
    }
    if (pila->planos >= 0.0) {
      /* `planos = 0,6` en serve.conf: la imagen sale en dos laminas con su propio desplazamiento
         en vez del relieve continuo; 0 lo deja como siempre */
      std::snprintf(valor, sizeof valor, "%.3f", pila->planos);
      args.push_back("--planos");
      args.push_back(valor);
      std::fprintf(stderr, "[serve] dos planos = %s (0 = relieve continuo, 1 = laminas planas)\n", valor);
    }
    if (pila->muestras >= 0) {
      /* `muestras = 1` en serve.conf: el tramo del centro recibe su propia muestra, asi se ve
         solido en lugar de degradado; 0 es la rampa continua de siempre */
      const std::string vm = std::to_string(pila->muestras);
      args.push_back("--muestras");
      args.push_back(vm);
      std::fprintf(stderr, "[serve] muestras intermedias = %s (0 = rampa continua)\n", vm.c_str());
    }
  }
  if (pila) {
    /* la carpeta de los mapas de correccion de este panel y el volcado de depuracion si van en
       serve.conf: esos dos no son numeros de la trama, asi que tambien van en modo de legado */
    if (!pila->calib.empty()) { args.push_back("--calib-dir"); args.push_back(pila->calib); }
    /* frecuencia del panel durante el 3D: `hz = 165` en serve.conf (el visor elige el modo 4K que mas
       se acerque sin pasarse). Sin la linea, 120 como siempre. */
    if (pila->hz > 0.0) {
      char hzTxt[24];
      std::snprintf(hzTxt, sizeof hzTxt, "%.2f", pila->hz);
      args.push_back("--hz");
      args.push_back(hzTxt);
    }
    if (!pila->volcado.empty()) {
      /* `volcado = /tmp/chk` en serve.conf: el visor guarda la trama, el mapa de profundidad y
         el origen, y NO sale: la sesion sigue en el panel mientras se mira. */
      args.push_back("--dump");
      args.push_back(pila->volcado);
      args.push_back("--volcado-seguir");
    }
  }
  if (rastreo) {
    /* la camara IR del monitor sigue los ojos y la trama se recalcula; sin esto la
       trama vive para la posicion de ojo nominal de fabrica */
    args.push_back("--track");
    if (suavizado > 0.0f) {
      char valor[32];
      std::snprintf(valor, sizeof valor, "%.3f", suavizado);
      args.push_back("--track-suavizado");
      args.push_back(valor);
    }
    if (pila) {
      if (!pila->camara.empty()) { args.push_back("--track-dev"); args.push_back(pila->camara); }
    }
    if (pila && !legadoPuesto) {
      /* con webcam normal la distancia sale del tamano de la cara: el campo y el
         ancho de cara se pueden ajustar desde serve.conf sin recompilar. En modo de
         legado nada de esto se pasa: ahi la cara se mide con las dos vistas de la
         camara del monitor, con el margen de error de reproyeccion ancho. */
      if (!pila->camara.empty()) { args.push_back("--track-dev"); args.push_back(pila->camara); }
      if (!pila->mono.empty() && pila->mono != "auto") { args.push_back("--track-mono"); args.push_back(pila->mono); }
      char valor[32];
      std::snprintf(valor, sizeof valor, "%.1f", pila->campo);
      args.push_back("--track-campo");
      args.push_back(valor);
      std::snprintf(valor, sizeof valor, "%.1f", pila->cara);
      args.push_back("--track-cara");
      args.push_back(valor);
    }
  }
  /* video normal: la red de profundidad decide cuanto desplazar cada ojo */
  if (modo == "conv") args.push_back("--conver");
  /* video dividido suavizado (v0.83): el conversor, pero solo con la imagen izquierda de la ventana */
  if (modo == "suave") args.push_back("--suave");
  /* y el boton de video dividido de la pastilla hace lo mismo que los ajustes */
  if (pila && !pila->divididoSuave) args.push_back("--sin-suavizado");
  if (pila && pila->divididoAuto) args.push_back("--suavizado-auto");
  /* La red y sus numeros viajan tambien con el video dividido (v0.79): desde la pastilla se puede
     pasar a video normal en marcha, y sin esto el visor abria la otra red, no la de serve.conf.
     Pasarlas no abre nada: la red solo se carga cuando el conversor se enciende. */
  if (modo == "conv" || modo == "suave" || ((modo == "sbs" || modo == "tb") && !legadoPuesto)) {
    /* `--modelo` enciende el conversor por si solo: con el video dividido tiene que esperar apagado
       (en la 0.79 faltaba esto y el video dividido arrancaba convertido como si fuera normal) */
    if (modo != "conv" && modo != "suave") args.push_back("--conversor-en-espera");
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
    if (pila && !pila->texto.empty() && pila->texto != "auto") {
      /* texto = no en serve.conf deja los rotulos con la profundidad que les da la red; sirve para
         comparar en el panel el deslizamiento de los subtitulos */
      args.push_back("--texto-panel");
      args.push_back(pila->texto);
    }
  }
  std::vector<char *> av;
  for (std::string &a : args) av.push_back(const_cast<char *>(a.c_str()));
  av.push_back(nullptr);
  ::setenv("GLFW_APPLICATION_ID", "odysseyvk", 1);
  if (pila) {
    /* los numeros de medicion van por entorno porque el visor ya los lee asi; no hace falta
        recompilar ni lanzar el visor a mano para probar un A/B en el panel */
    char valor[32];
    if (pila->relieve >= 0.0) {
      std::snprintf(valor, sizeof valor, "%.4f", pila->relieve);
      ::setenv("ODYSSEYVK_RELIEVE", valor, 1);
    }
    if (!pila->saliencia.empty())
      ::setenv("ODYSSEYVK_SALIENCIA", (pila->saliencia == "si" || pila->saliencia == "1") ? "1" : "0", 1);
    /* cantos finos (v0.89): de fabrica. `cantos = viejo` vuelve al camino de antes, para comparar */
    if (!pila->cantos.empty()) ::setenv("ODYSSEYVK_CANTOS", pila->cantos.c_str(), 1);
    if (!pila->hdrEn3d.empty())
      ::setenv("ODYSSEYVK_HDR_3D", (pila->hdrEn3d == "si" || pila->hdrEn3d == "1") ? "1" : "0", 1);
    /* v1.11: la sincronizacion adaptativa se APAGA de fabrica durante el 3D. MEDIDO 26-nov con
       Cyberpunk: con el VRR puesto el refresco seguia al juego y el visor esperaba su imagen libre
       15-35 ms; 25 % de las presentaciones tarde con el juego bajo 100 cuadros. Apagado: 165 Hz fijos,
       0,1 % tarde, y Rodolfo lo noto fluido. `vrr_en_3d = si` lo deja como en el escritorio. */
    ::setenv("ODYSSEYVK_VRR_3D", (pila->vrrEn3d == "si" || pila->vrrEn3d == "1") ? "1" : "0", 1);
    if (!pila->barra2d.empty())
      ::setenv("ODYSSEYVK_BARRA_2D",
               (pila->barra2d == "no" || pila->barra2d == "0") ? "0" : (pila->barra2d == "entera" ? "entera" : "1"), 1);
    if (!pila->rastreoFino.empty())
      ::setenv("ODYSSEYVK_RASTREO_FINO", (pila->rastreoFino == "no" || pila->rastreoFino == "0") ? "0" : "1", 1);
    if (!pila->escala3d.empty())
      ::setenv("ODYSSEYVK_ESCALA_3D", (pila->escala3d == "1" || pila->escala3d == "1,0" || pila->escala3d == "1.0") ? "1" : "escritorio", 1);
    if (!pila->atajosGlobales.empty())
      ::setenv("ODYSSEYVK_ATAJOS", (pila->atajosGlobales == "no" || pila->atajosGlobales == "0") ? "0" : "1", 1);
    /* la rama experimental: lo que no esta puesto se QUITA del entorno, que el servicio es el mismo
       proceso de una sesion a otra y una prueba apagada en el panel tiene que dejar de llegar */
    {
      const int L = std::atoi(pila->expRed.c_str());
      if (L >= 294) {
        const int w = (L / 14) * 14, h = (int)std::lround((double)L * 9.0 / 16.0 / 14.0) * 14;
        std::snprintf(valor, sizeof valor, "%dx%d", w, h);
        ::setenv("ODYSSEYVK_RED_ENTRADA", valor, 1);
      } else {
        ::unsetenv("ODYSSEYVK_RED_ENTRADA");
      }
      const int M = std::atoi(pila->expMapa.c_str());
      if (M >= 294 && M != 518) ::setenv("ODYSSEYVK_MAPA", std::to_string((M / 14) * 14).c_str(), 1);
      else ::unsetenv("ODYSSEYVK_MAPA");
      if (pila->expTope == "si" || pila->expTope == "1") ::setenv("ODYSSEYVK_TOPE_ARRASTRE", "1", 1);
      else ::unsetenv("ODYSSEYVK_TOPE_ARRASTRE");
      if (pila->expPrioridad == "alta" || pila->expPrioridad == "tiempo_real")
        ::setenv("ODYSSEYVK_PRIORIDAD", pila->expPrioridad.c_str(), 1);
      else
        ::unsetenv("ODYSSEYVK_PRIORIDAD");
      if (pila->expAnticipacion > 0.0) {
        std::snprintf(valor, sizeof valor, "%.1f", pila->expAnticipacion);
        ::setenv("ODYSSEYVK_ANTICIPACION", valor, 1);
      } else {
        ::unsetenv("ODYSSEYVK_ANTICIPACION");
      }
    }
    if (pila->pivote >= 0.0) {
      std::snprintf(valor, sizeof valor, "%.4f", pila->pivote);
      ::setenv("ODYSSEYVK_PIVOTE", valor, 1);
    }
    if (pila->relieveLocal >= 0.0) {
      std::snprintf(valor, sizeof valor, "%.4f", pila->relieveLocal);
      ::setenv("ODYSSEYVK_RELIEVE_LOCAL", valor, 1);
    }
    if (pila->mapasSeg > 0.0) {
      std::snprintf(valor, sizeof valor, "%.2f", pila->mapasSeg);
      ::setenv("ODYSSEYVK_MAPAS_SEG", valor, 1);
    }
    if (pila->detalle >= 0.0) {
      std::snprintf(valor, sizeof valor, "%.4f", pila->detalle);
      ::setenv("ODYSSEYVK_DETALLE", valor, 1);
    }
    if (pila->paralaje < 900.0) {
      std::snprintf(valor, sizeof valor, "%.4f", pila->paralaje);
      ::setenv("ODYSSEYVK_PARALAJE", valor, 1);
    }
    if (pila->focoIir > 0.0) {
      std::snprintf(valor, sizeof valor, "%.4f", pila->focoIir);
      ::setenv("ODYSSEYVK_FOCO_IIR", valor, 1);
    }
    if (pila->crosstalkBase >= 0.0) {
      std::snprintf(valor, sizeof valor, "%.5f", pila->crosstalkBase);
      ::setenv("ODYSSEYVK_CROSSTALK_BASE", valor, 1);
    }
    if (pila->temblor >= 0.0) {
      std::snprintf(valor, sizeof valor, "%.4f", pila->temblor);
      ::setenv("ODYSSEYVK_DITHER", valor, 1);
    }
    if (pila->canto > 0.0) {
      std::snprintf(valor, sizeof valor, "%.5f", pila->canto);
      ::setenv("ODYSSEYVK_CANTO", valor, 1);
    }
    if (pila->faseTemblor >= 0.0) {
      std::snprintf(valor, sizeof valor, "%.4f", pila->faseTemblor);
      ::setenv("ODYSSEYVK_FASE_TEMBLOR", valor, 1);
    }
    /* `rampa_x` / `rampa_y`: aprieta o afloja el paso de la rejilla escrita, en ciclos por pixel
       de panel. Vale para cuadradlo con el paso real de la lente de ESTE panel; el valor medido
       para el nuestro es -0,00283 (ver tools/paso_rejilla.py). No es lo mismo que `paso`. */
    if (pila->rampaX < 900.0) {
      std::snprintf(valor, sizeof valor, "%.6f", pila->rampaX);
      ::setenv("ODYSSEYVK_RAMPA_X", valor, 1);
      std::fprintf(stderr, "[serve] rampa_x = %s ciclos por pixel de panel\n", valor);
    }
    if (pila->rampaY < 900.0) {
      std::snprintf(valor, sizeof valor, "%.6f", pila->rampaY);
      ::setenv("ODYSSEYVK_RAMPA_Y", valor, 1);
      std::fprintf(stderr, "[serve] rampa_y = %s ciclos por pixel de panel\n", valor);
    }
    /* `fondo` / `frente`: cuanto se estira el relieve a cada lado del plano de apoyo (1 = como
       siempre). Con `fondo = 1,4` el fondo se va mas atras sin tocar los primeros planos; es la
       pareja que en el programa de Windows viene por dentro, en las bandas de su config. */
    if (pila->ganFondo >= 0.0) {
      std::snprintf(valor, sizeof valor, "%.3f", pila->ganFondo);
      ::setenv("ODYSSEYVK_GAN_FONDO", valor, 1);
      std::fprintf(stderr, "[serve] capa fondo = %s (1 = sin cambiar)\n", valor);
    }
    if (pila->ganFrente >= 0.0) {
      std::snprintf(valor, sizeof valor, "%.3f", pila->ganFrente);
      ::setenv("ODYSSEYVK_GAN_FRENTE", valor, 1);
      std::fprintf(stderr, "[serve] capa frente = %s (1 = sin cambiar)\n", valor);
    }
    /* `empuje_fondo` / `empuje_frente`: desplazamiento fijo de cada capa, en puntos de panel. Es
       lo que en la config de Windows es `far_diff_popup` en las bandas y `depth_offset` en la capa
       de graficos superpuestos: llevar una banda entera a un plano concreto. */
    if (pila->empFondo <= 15.0) {
      std::snprintf(valor, sizeof valor, "%.3f", pila->empFondo);
      ::setenv("ODYSSEYVK_EMP_FONDO", valor, 1);
      std::fprintf(stderr, "[serve] empuje fondo = %s puntos (0 = sin cambiar)\n", valor);
    }
    if (pila->empFrente <= 15.0) {
      std::snprintf(valor, sizeof valor, "%.3f", pila->empFrente);
      ::setenv("ODYSSEYVK_EMP_FRENTE", valor, 1);
      std::fprintf(stderr, "[serve] empuje frente = %s puntos (0 = sin cambiar)\n", valor);
    }
  }
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
   comando `odysseyvk ctl aparece`. */
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
    if (w.appId != "odysseyvk" || w.title != NOMBRE_APP) continue;  /* el visor, no la tarjeta */
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
  std::string titulo;   /* vacio = la pregunta de fabrica de la tarjeta */
  std::string cuerpo = "Nombre del vídeo de prueba — Navegador";
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
   ~/.local/state/odysseyvk con la hora del reloj monotono; caduca, asi que si el hub se va el visor
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

/* Devolver el panel a 2D si queda una instantanea sin consumir. La instantanea la crea el visor al
   poner el monitor en modo 3D y la borra al restaurar, asi que si aun existe es que una sesion se fue
   de golpe: el visor revento, lo matamos, o el servicio se paro antes de tiempo. Sin esto el monitor
   se queda en modo 3D, con la lente encendida y el scaler en 3D hasta que alguien cierre a mano. */
static void devolverPanelHuerfano(const char *motivo) {
  PanelState snap{};
  std::vector<Placement> otras;
  std::string snapPath, e;
  if (!Panel::loadSnapshot(snap, &snapPath, &e, &otras)) return;
  std::printf("[serve] %s: quedaba el panel guardado en modo 3D (modo %d %s, escala %g): lo devolvemos\n",
              motivo, snap.modeIndex, snap.modeRes.c_str(), snap.scale);
  std::fflush(stdout);
  const std::string lp = Lens::findPort();
  Lens lens;
  if (!lp.empty() && lens.open(lp)) {
    if (!lens.authenticated()) lens.authenticate();
    if (lens.off()) std::printf("[lente] devuelta a 2D\n");
  }
  const std::string sd = Scaler::findDevice();
  Scaler sc;
  if (!sd.empty() && sc.open(sd)) {
    if (sc.set3d(false)) std::printf("[scaler] modo 3D apagado\n");
  }
  if (Panel::restore(snap, &e, otras.empty() ? nullptr : &otras)) {
    Panel::clearSnapshot();
    std::printf("[panel] restaurado\n");
    for (const Placement &q : otras)
      std::printf("[panel] %s devuelta a %d,%d\n", q.name.c_str(), q.x, q.y);
  } else {
    std::fprintf(stderr, "[aviso] no se pudo restaurar el panel: %s\n", e.c_str());
  }
  std::fflush(stdout);
  std::fflush(stderr);
}

static int cmdServe(int argc, char **argv) {
#ifndef HAVE_WAYLAND_WM
  (void)argc; (void)argv;
  std::fprintf(stderr, "compilado sin soporte de ventanas Wayland (hace falta wayland-client)\n");
  return 1;
#else
  ServeCfg cfg;
  cfg.ignore = {"odysseyvk", "plasmashell", "plasma", "kscreen", "polkit", "xwayland", "krunner", "dolphin"};
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

  /* Que numeros de trama ha leido el servicio, para no tener que adivinar si la ultima linea de
     serve.conf entro o se la comio un tope de rango. -1 en los crudos significa sin ajustar. */
  {
    char rx[32], ry[32], pl[32], mu[32];
    if (cfg.rampaX < 900.0) std::snprintf(rx, sizeof rx, "%.6f", cfg.rampaX); else std::snprintf(rx, sizeof rx, "sin ajustar");
    if (cfg.rampaY < 900.0) std::snprintf(ry, sizeof ry, "%.6f", cfg.rampaY); else std::snprintf(ry, sizeof ry, "sin ajustar");
    if (cfg.planos >= 0.0) std::snprintf(pl, sizeof pl, "%.2f", cfg.planos); else std::snprintf(pl, sizeof pl, "apagado");
    if (cfg.muestras >= 0) std::snprintf(mu, sizeof mu, "%d", cfg.muestras); else std::snprintf(mu, sizeof mu, "sin ajustar");
    std::printf("[serve] trama cruda (-1 = sin ajustar): fase %.4f paso %.4f filtro %.4f temblor %.4f"
                " canto %.4f fase_temblor %.4f · rampa_x %s rampa_y %s · dos planos %s muestras %s\n",
                cfg.fase, cfg.paso, cfg.filtro, cfg.temblor, cfg.canto, cfg.faseTemblor, rx, ry, pl, mu);
    std::fflush(stdout);
  }

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
  /* Arrancar con el monitor en modo 3D y sin ningun visor vivo significa que la sesion anterior se
     fue de golpe (reinicio del servicio, visor matado). Devolvemos el panel antes de empezar a mirar
     ventanas, si no el escritorio queda en 3D sin nadie que lo apague. */
  {
    wm.settle(120, 12);
    bool visorVivo = false;
    for (const WindowInfo &w : wm.windows())
      if (w.mapped && w.appId == "odysseyvk") visorVivo = true;
    if (!visorVivo) devolverPanelHuerfano("el monitor seguia en modo 3D de una sesion anterior");
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
                  "pantalla=DP-4 en ~/.config/odysseyvk/serve.conf\n");
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
  std::string accionApp;                       /* de que aplicacion era la tarjeta que se contesto */
  bool accionGuardada = false;                 /* y si se marco «no volver a preguntar» */
  double avisoT = 0.0;
  std::string tarjetaUuid, tarjetaApp;         /* ventana a la que le pusimos la tarjeta */
  double tarjetaT = 0.0;
  /* el icono de la bandeja (v1.03): clic = panel de ajustes, clic central = la pestana Juegos */
  Bandeja bandeja;
  double bandejaIntento = -100.0;
  bandeja.alPicar([&](const std::string &como) {
    const pid_t p = ::fork();
    if (p == 0) {
      ::setsid();
      if (como == "medio") ::execl(exe.c_str(), exe.c_str(), "ajustes", "--juegos", (char *)nullptr);
      else ::execl(exe.c_str(), exe.c_str(), "ajustes", (char *)nullptr);
      ::_exit(127);
    }
    std::printf("[bandeja] %s: el panel de ajustes\n", como.c_str());
    std::fflush(stdout);
  });
  /* la actualizacion (v0.95): se mira la version de GitHub medio minuto despues de arrancar y luego
     cada seis horas; con una mas nueva, la tarjeta de la orilla lo avisa (una vez por version) */
  double proximaComprobacion = 0.0;
  bool tarjetaActualizacion = false;
  double tarjetaActT = 0.0;
  std::string versionAvisada;
  readSmallFile(rutaEstadoOdyssey("actualizacion-avisada"), versionAvisada);
  versionAvisada = strTrim(versionAvisada);
  /* el visor que se reabre tras una actualizacion: `odysseyvk actualizar` deja aqui la ventana y el
     modo de la sesion que corto al reiniciar el servicio; vale tres minutos */
  std::string reabrirUuid, reabrirModo;
  /* Los juegos (steam_app_*) al arrancar pasan por su lanzador, una ventana de error o de aviso, y la
     suya propia, que se pone a pantalla completa, se minimiza y vuelve en un par de segundos (MEDIDO
     con Cyberpunk, 26-nov 00:11: pantalla completa, 3D lanzado al segundo, minimizada al segundo
     siguiente, sesion cerrada, y otra vez; y al final el servicio le quito la pantalla completa al
     juego como si fuera un navegador). Desde v0.98: a un juego solo se le lanza el 3D cuando lleva
     4 s seguidos a pantalla completa, una minimizacion de menos de 6 s no cierra la sesion, y nunca
     se le quita la pantalla completa. */
  auto esJuego = [](const std::string &appId) {
    const std::string a = strLower(appId);
    return a.rfind("steam_app_", 0) == 0 || a == "gamescope";
  };
  /* El aviso de la tarjeta (v1.00): un titulo, renglones y «Entendido». Lo usan el detector de
     caidas de un juego y el del visor que un juego a pantalla completa manda detras. */
  bool tarjetaAviso = false;
  double tarjetaAvisoT = 0.0;
  std::string hijoApp;          /* la aplicacion de la sesion en curso */
  int visorEscondidas = 0;      /* cuantas veces un juego mando el visor detras en esta sesion */
  /* v1.07: el texto del aviso se guarda como receta (los textos en español y sus datos), no ya
     traducido: si se cambia el idioma con la tarjeta puesta, se vuelve a escribir en el nuevo.
     Antes salia «了解» en el boton y el titulo y los renglones seguian en español. */
  std::function<void(std::string &, std::vector<std::string> &)> avisoTexto;
  auto mostrarAviso = [&](std::function<void(std::string &, std::vector<std::string> &)> texto, double ahora) {
    if (!hayTarjeta || !ov || ov->askVisible()) return;
    avisoTexto = std::move(texto);
    std::string titulo;
    std::vector<std::string> lineas;
    avisoTexto(titulo, lineas);
    ov->hud().state().askAviso = titulo;
    ov->hud().state().askAvisoLineas = lineas;
    ov->showAsk(std::string(), std::string());
    tarjetaAviso = true;
    tarjetaAvisoT = ahora;
    std::printf("[serve] tarjeta de aviso: %s\n", titulo.c_str());
    std::fflush(stdout);
  };
  std::map<std::string, double> fullVisto;   /* desde cuando lleva cada ventana a pantalla completa */
  double videoMinimizadaDesde = 0.0;
  {
    std::string txt;
    if (readSmallFile(rutaEstadoOdyssey("reabrir"), txt)) {
      ::unlink(rutaEstadoOdyssey("reabrir").c_str());
      std::vector<std::string> l;
      size_t ini = 0;
      while (ini <= txt.size()) {
        size_t fin = txt.find('\n', ini);
        if (fin == std::string::npos) fin = txt.size();
        l.push_back(strTrim(txt.substr(ini, fin - ini)));
        ini = fin + 1;
      }
      if (l.size() >= 3 && std::time(nullptr) - std::atol(l[2].c_str()) < 180 && !l[0].empty() && l[0] != "-") {
        reabrirUuid = l[0];
        reabrirModo = l[1];
        std::printf("[serve] tras la actualizacion: si %s sigue a pantalla completa se reabre en %s\n", reabrirUuid.c_str(),
                    reabrirModo.c_str());
      }
    }
  }
  pid_t hijo = -1;
  std::string hijoUuid;
  double intentoAparecer = 0.0;   /* cuando intentamos por ultima vez devolver la ventana minimizada */
  bool avisoAparecerHecho = false;
  double intentoEncima = 0.0;     /* cuando reforzamos el "encima de todo" del visor */
  bool avisoEncimaHecho = false;
  double fueraVideoDesde = 0.0;   /* cuando el video empezo a estar fuera de su sitio */
  bool cerrandoSesion = false;    /* ya avisamos al visor que se vaya; estamos esperando que muera */
  double hijoDesde = 0.0;         /* cuando arranco la sesion en curso */
  int reafirmadas = 0;            /* veces que le devolvimos la pantalla completa al video en esta sesion */
  double reacomodoDesde = 0.0;    /* cuando fue el ultimo reintento: no machacar al compositor */
  bool avisoChicoHecho = false;   /* el aviso de "el video no cubre el lienzo" va una sola vez */
  bool videoActivaAntes = false;  /* el video tenia el foco en el tick anterior */
  double pilaDesde = 0.0;         /* ultimo momento en que pedimos el orden de apilamiento */
  int apilados = 0;               /* veces que volvimos a poner el visor por encima del video */
  bool avisoPilaHecho = false;    /* el aviso de "no quiso ponerlo encima" va una sola vez */
  double debajoDesde = 0.0;       /* ultimo momento en que le pusimos KeepBelow al video */
  bool debajoAvisado = false;     /* el aviso de "el video pasa por debajo" va una sola vez */
  std::string firmaVideo;         /* estados y tamano del video la ultima vez que lo miramos, para dar
                                     cuenta de cualquier cambio con su reloj */
  /* Vigia del navegador por MPRIS (ver el bloque que lo usa mas abajo). */
  Mpris navegador;               /* reproductor MPRIS de la ventana del navegador, si lo hay */
  bool navegadorAbierto = false; /* ya intentamos abrir el bus en esta sesion */
  bool navegadorAvisado = false; /* el aviso de "no hay reproductor MPRIS" va una sola vez */
  std::string estadoNavegador;   /* ultimo estado que nos dijo el navegador ("Playing", "Paused"...) */
  double paradoDesde = 0.0;      /* cuando empezo a decir "Parado" (0 = no lo esta diciendo) */
  /* Otras ventanas que bajamos con KeepBelow mientras dura la sesion: guardamos su uuid y si tenian
     KeepAbove puesto, para devolvérselo tal cual. */
  std::vector<std::pair<std::string, bool>> bajadas;
  double otrosDesde = 0.0;        /* ultimo momento en que cuidamos a las demas ventanas */
  bool otrosAvisado = false;      /* el aviso de por que las bajamos va una sola vez */
  double enfriadoHasta = 0.0;     /* cerramos una sesion hace poco: hasta aqui no se ofrece otra */
  int muertesSeguidas = 0;        /* sesiones que murieron apenas empezaron */
  /* La ventana del video de una sesion que se cerro PORQUE EL VIDEO SE FUE. Ahi es justo quitarle
     la pantalla completa al navegador (no cuando uno elige 2D plano, entrega la pantalla con Tab o
     saca el visor con ESC: en esos casos el video sigue sonando y la pantalla completa es suya). */
  bool videoSeFue = false;
  int reposoW = 0, reposoH = 0;   /* tamano logico del panel SIN sesion 3D (para saber si el 3D lo cambia) */
  bool vueltaPendiente = false;   /* se dejo la ventana a pantalla completa al cerrar: quitarsela al volver el monitor */
  std::string caidaUuid;          /* ventana de navegador a la que KWin saco de pantalla completa al empezar */
  double caidaHora = -1e9;        /* y cuando: si repite pronto, se le repone como antes */
  std::string seFueUuid;          /* la ventana a la que hay que devolverle el estado */
  double seFueDesde = 0.0;        /* cuando cerro la sesion */
  double fullDesde = 0.0;         /* cuando le quitamos la pantalla completa por ultima vez */
  int fullQuitadas = 0;           /* intentos hechos: no pelear con el navegador para siempre */
  bool fullAvisado = false;       /* el aviso de "el navegador la vuelve a poner" va una sola vez */

  /* El overlay FORZADO en el escritorio del monitor. Lo pide la ventana de ajustes (el boton escribe
     `overlay = 1` en el fichero de ajuste) o `odysseyvk ctl escritorio poner`: el panel pasa a 3D y el
     visor teje su patron de trama, sin video detras. Sirve para mirar y afinar la trama, las franjas y
     las rampas con el escritorio por delante, sin tener que abrir ningun video. Es una ORDEN, no la
     verdad: solo hacemos algo cuando el numero CAMBIA en el fichero y cuando lo que pide no esta hecho
     ya, asi que el mismo boton la quita (`overlay = 0`) y un clic de mas no vuelve a montarla. */
  bool forzado = false;             /* la sesion en curso es la forzada del escritorio */
  ajuste::Lector lectorOrden;       /* el fichero de ajuste, mirado solo cuando cambia */
  bool primerOrden = true;          /* la primera lectura no es una orden: puede venir vieja */
  double hubEstadoDesde = 0.0;      /* cada cuanto contamos en el fichero `hub` que sesion tenemos */

  /* Bajar a las demas ventanas mientras dura la sesion. El video va con KeepBelow para que un click en
     su barra de tiempos no suba el navegador por encima de la trama, pero KeepBelow lo manda al fondo
     del apilamiento ENTERO: si ademas hay otra ventana normal, el click que el visor deja pasar se lo
     queda ESA ventana y el video no contesta a nada (medido con `ctl apilamiento`: la ventana maximizada
     del lanzador de juegos quedaba en la posicion 9 de 10, por encima del video ya bajado). Bajando
     tambien a las demas, el video queda arriba dentro del grupo de abajo —era la ventana activa, o sea
     la ultima de su grupo— y el click llega a su barra de tiempos igual. Al terminar se devuelve todo. */
  auto bajarDemas = [&]() -> int {
    int hechas = 0;
    for (const WindowInfo &w : wm.windows()) {
      if (!w.mapped || w.minimized() || w.uuid == hijoUuid) continue;
      if (w.appId == "odysseyvk") continue;              /* el visor y la tarjeta, no */
      if (w.states & (WindowInfo::KeepBelow | WindowInfo::KeepAbove)) continue;  /* ya tiene orden propio */
      std::string e7;
      if (wm.setState(w.uuid, WindowInfo::KeepBelow, true, &e7)) {
        bajadas.emplace_back(w.uuid, (w.states & WindowInfo::KeepAbove) != 0);
        ++hechas;
      }
    }
    return hechas;
  };
  auto devolverDemas = [&]() {
    std::string e7;
    for (const auto &u : bajadas) {
      if (!wm.setState(u.first, WindowInfo::KeepBelow, false, &e7))
        std::printf("[ventana] no se le pudo quitar el debajo a %s: %s\n", u.first.c_str(), e7.c_str());
      else if (u.second && !wm.setState(u.first, WindowInfo::KeepAbove, true, &e7))
        std::printf("[ventana] no se le pudo devolver el encima a %s: %s\n", u.first.c_str(), e7.c_str());
    }
    bajadas.clear();
    std::fflush(stdout);
  };

  nt.setOnAction([&](const std::string &id, const std::string &action) {
    auto it = idUuid.find(id);
    if (it == idUuid.end()) return;
    accion = strLower(action);
    avisoUuid = it->second;
    std::printf("[serve] respuesta %s -> %s\n", action.c_str(), it->second.c_str());
    std::fflush(stdout);
  });

  /* Las dos redes del boton 1 / 2 del visor, cada una en su boton segun el nombre del archivo:
     1 = las que trajo el programa de Samsung (mde_*, Disp_*), 2 = la nativa (Depth Anything).
     Si en modelo= se puso la nativa se reconoce y se mueve sola al boton 2. La eleccion que hizo
     el usuario la guarda el visor en ~/.config/odysseyvk/red-elegida y aca se lee para arrancar con esa. */
  auto disponible = [](const std::string &r) { return !r.empty() && fileExists(r); };
  std::string redActiva, redOtra;
  int redElegida = 2;
  /* Se vuelve a hacer cuando cambia `modelo =` en serve.conf (el panel de ajustes lo cambia, v0.92):
     vale para el siguiente visor. Sin `modelo =`, o con la abierta en el, se arranca con la abierta
     (Depth Anything V2): es la de fabrica. Con la de Samsung puesta ahi, con esa. */
  auto elegirRedes = [&] {
    /* Las dos redes, sin que nadie tenga que decir nada (v0.94): la abierta es la de siempre
       (Depth Anything V2) y, si en la misma carpeta esta la del programa de Samsung
       (`mde_large.onnx`), se detecta sola y arranca con ella (boton 1); la abierta queda en el
       boton 2. `modelo =` sigue valiendo para una ruta a mano: segun el nombre del archivo va a
       un boton o al otro. */
    const std::string carpeta = rutaRedNuestra().substr(0, rutaRedNuestra().rfind('/'));
    std::string rSamsung = carpeta + "/mde_large.onnx";
    std::string rNuestra = cfg.nuestra.empty() ? rutaRedNuestra() : cfg.nuestra;
    if (!cfg.modelo.empty()) (slotRed(cfg.modelo) == 1 ? rSamsung : rNuestra) = cfg.modelo;
    const std::string ruta1 = disponible(rSamsung) ? rSamsung : std::string();
    const std::string ruta2 = disponible(rNuestra) ? rNuestra : std::string();
    redElegida = ruta1.empty() ? 2 : 1;
    redActiva = redElegida == 1 ? ruta1 : ruta2;
    redOtra = redElegida == 1 ? ruta2 : ruta1;
    std::printf("[serve] redes de profundidad: boton 1 = %s · boton 2 = %s\n",
                ruta1.empty() ? "no hay" : ruta1.c_str(), ruta2.empty() ? "no hay" : ruta2.c_str());
    if (redActiva.empty())
      std::printf("[serve]   sin ninguna red no hay video normal a 3D (el dividido si)\n");
    else
      std::printf("[serve]   arrancamos con la %d (%s); en el visor se cambia con las teclas 1 y 2 o con el boton\n",
                  redElegida, redElegida == 1 ? "la de Samsung, encontrada en la carpeta" : "abierta");
  };
  elegirRedes();
  /* Sin ninguna red no hay video normal a 3D: se abre el panel de ajustes, que arriba del todo dice
     que hace falta descargarla para tener datos de profundidad y trae el boton que lo hace (v0.92;
     desde v0.94 cada vez que el servicio arranca sin red, no solo la primera). */
  if (redActiva.empty()) {
    std::printf("[serve] no hay ninguna red de profundidad: %s el panel de ajustes para descargar la abierta "
                "(o `odysseyvk ctl red descargar`)\n", dry ? "(dry) se abriria" : "se abre");
    std::fflush(stdout);
    if (!dry) {
      const pid_t p = ::fork();
      if (p == 0) {
        ::execl(exe.c_str(), exe.c_str(), "ajustes", (char *)nullptr);
        ::_exit(127);
      }
    }
  }
  {
    std::string aviso;
    if (cfg.rastreo && IrCam::findDevices().empty()) {
      aviso = " · ojo: no aparece la camara 04e8:20d7 (3D Stereo WebCam)";
      std::string alterna;
      if (pickCameraDevice(alterna, nullptr) && !alterna.empty())
        aviso += "; en su lugar se usara " + alterna + " midiendo el tamano de la cara";
    }
    std::printf("[serve]   rastreo %s%s\n",
                cfg.rastreo ? "activo (la camara IR sigue los ojos)"
                            : "apagado (trama para la posicion de ojo nominal de fabrica)",
                aviso.c_str());
  }
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
    bandeja.atender();
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
        const bool eraForzada = forzado;   /* la del escritorio la quito uno a proposito */
        forzado = false;
        std::printf("[serve] sesion 3D terminada (%s)\n",
                    hijoUuid.empty() ? "el overlay del escritorio" : hijoUuid.c_str());
        std::fflush(stdout);
        /* Si la sesion se cerro porque el video se fue, esa ventana queda apuntada unos segundos:
           es la que hay que devolver a estado normal (abajo se explica por que). */
        if (videoSeFue && !hijoUuid.empty()) {
          if (seFueUuid != hijoUuid) {   /* si la vigilia ya empezo al volver el monitor, no se reinicia la cuenta */
            seFueUuid = hijoUuid;
            fullDesde = 0.0;
            fullQuitadas = 0;
            fullAvisado = false;
          }
          seFueDesde = now;              /* los 6 s cuentan desde que el visor ya no esta */
        }
        videoSeFue = false;
        vueltaPendiente = false;
        AtajosGlobales::desactivar();    /* por si el visor murio sin soltar los atajos globales */
        /* El video estuvo debajo de todo para que un click no tapara la trama: al terminar la sesion
           vuelve a su lugar, si no el usuario se queda con el video escondido detras de las ventanas. */
        {
          std::string e6;
          WindowInfo videoAntes{};
          if (wm.find(hijoUuid, videoAntes) && videoAntes.mapped &&
              (videoAntes.states & WindowInfo::KeepBelow) &&
              !wm.setState(hijoUuid, WindowInfo::KeepBelow, false, &e6))
            std::printf("[ventana] no se le pudo quitar el debajo al video %s: %s\n",
                        hijoUuid.c_str(), e6.c_str());
          std::fflush(stdout);
        }
        devolverDemas();          /* las otras ventanas vuelven a su lugar del apilamiento */
        hijo = -1;
        hijoUuid.clear();
        fueraVideoDesde = 0.0;
        cerrandoSesion = false;
        reafirmadas = 0;          /* la proxima sesion empieza con sus tres intentos frescos */
        reacomodoDesde = 0.0;
        avisoChicoHecho = false;
        videoActivaAntes = false;
        pilaDesde = 0.0;
        apilados = 0;
        avisoPilaHecho = false;
        debajoDesde = 0.0;
        debajoAvisado = false;
        otrosDesde = 0.0;
        otrosAvisado = false;
        firmaVideo.clear();
        estadoNavegador.clear();
        paradoDesde = 0.0;
        navegadorAbierto = false;
        navegadorAvisado = false;
        /* Backoff: al cambiar el modo del panel KWin remueve la ventana del video y hay instantes en
           que no figura a pantalla completa. Si acabamos de cerrar una sesion y la volvemos a lanzar
           al instante, entramos en un bucle de lanzar/cerrar que deja el monitor parpadeando. */
        if (eraForzada) {
          /* El overlay del escritorio lo quitamos nosotros con el boton: no es una muerte rara, no hay
             por que enfriar nada y el boton puede volver a montarlo al momento. */
          muertesSeguidas = 0;
          enfriadoHasta = now;
        } else {
          if (hijoDesde > 0.0 && now - hijoDesde < 20.0) ++muertesSeguidas;
          else muertesSeguidas = 0;
          enfriadoHasta = now + 8.0;
          if (muertesSeguidas >= 2) {
            muertesSeguidas = 0;
            enfriadoHasta = now + 60.0;
            std::printf("[serve] la sesion se cerro apenas empezo: esperamos un minuto antes de volver"
                        " a ofrecer 3D\n");
            std::fflush(stdout);
          }
        }
        hijoDesde = 0.0;
        /* Red de seguridad: el visor, al irse por su pie, restaura el panel y borra la instantanea.
           Si la instantanea sigue ahi es que el visor se fue de golpe (SIGKILL, cuelgue) y el monitor
           se quedo en modo 3D con la lente encendida. */
        devolverPanelHuerfano("la sesion 3D se termino");
      }
    }

    /* La ventana de ajustes (o `odysseyvk ctl escritorio poner`) puede pedir el overlay montado EN EL
       ESCRITORIO del monitor, sin esperar a que nadie se ponga a pantalla completa: el panel pasa a 3D
       y el visor teje su patron de trama (el mismo camino de `odysseyvk test`), asi se ven y se afinan
       la trama, las franjas y las rampas con el escritorio por delante. NO capturamos la salida del
       monitor: ahi estaria nuestra propia ventana y la trama se tejeria a si misma. Un segundo clic en
       el boton escribe `overlay = 0` y lo quita; si ya esta como lo pide, no hacemos nada mas. */
    {
      std::map<std::string, double> ordenes;
      std::string modeloCanal;
      std::map<std::string, std::string> palabras;   /* las ordenes que llevan palabra: camara, mono */
      if (lectorOrden.cambio(ordenes, modeloCanal, &palabras)) {
        const auto it = ordenes.find("overlay");
        /* La PRIMERA vez que miramos el fichero al arrancar el servicio no es un cambio: puede venir
           con una orden vieja de cuando el equipo se apago o de un servicio que ya no esta. Montar el
           overlay del escritorio sin que nadie lo haya pedido en este arranque seria una sorpresa (le
           cambia el panel a 3D en plena mesa), asi que la primera lectura se pasa de largo. Los numeros
           del visor si se dejan pasar: esos se aplican cuando el visor se abre, y ahi si tiene sentido. */
        if (it != ordenes.end() && !primerOrden) {
          const bool quiere = it->second >= 0.5;
          if (quiere && !forzado) {
            if (hijo > 0) {
              std::printf("[serve] el overlay del escritorio espera: ya hay una sesion 3D abierta (%s)\n",
                          hijoUuid.empty() ? "sin ventana de video" : hijoUuid.c_str());
              std::fflush(stdout);
            } else if (dry) {
              std::printf("[serve] (dry) habria puesto el overlay del escritorio (patron de trama sin"
                          " video)\n");
              std::fflush(stdout);
            } else {
              /* sin modo y sin uuid: el patron de trama no necesita formato y no hay ventana de video
                 que vigilar; el resto (panel, lente, pastilla, rastreo) es el de siempre */
              hijo = spawnViewer(exe, std::string(), std::string(), cfg.red, cfg.cada, redActiva,
                                 cfg.pantalla, redOtra, cfg.rastreo, cfg.suavizado, &cfg, true);
              if (hijo > 0) {
                forzado = true;
                ajuste::registrar("overlay", "0", "1", redActiva, "el boton del overlay (o ctl escritorio)");
                hijoDesde = now;
                std::printf("[serve] overlay puesto en el escritorio de %s: patron de trama sin video."
                            " Se quita con el mismo boton, con ESC en la ventana o con `ctl escritorio"
                            " quitar`\n", cfg.pantalla.empty() ? "la pantalla del panel" : cfg.pantalla.c_str());
                std::fflush(stdout);
              } else {
                std::fprintf(stderr, "[serve] no se pudo lanzar el overlay del escritorio\n");
                std::fflush(stderr);
              }
            }
          } else if (!quiere && forzado && hijo > 0) {
            ajuste::registrar("overlay", "1", "0", redActiva, "el boton del overlay (o ctl escritorio)");
            std::printf("[serve] quitamos el overlay del escritorio (lo pidio la ventana de ajustes)\n");
            std::fflush(stdout);
            ::kill(hijo, SIGTERM);
          }
        }
        /* La camara del Facetracking: `camara = /dev/videoN` y, si hace falta, `mono = si|no|auto`.
           La ventana de ajustes deja ademas la eleccion escrita en serve.conf, asi que aca solo hace
           falta que el vigilante la recuerde para la proxima sesion: la camara la tiene abierta el
           visor, y cambiarla en plena sesion seria cerrarla y abrir otra con el rastreo encima. */
        bool elegida = false;
        std::string antesCamara = cfg.camara, antesMono = cfg.mono;
        const auto ic = palabras.find("camara");
        if (ic != palabras.end() && ic->second != cfg.camara) { cfg.camara = ic->second; elegida = true; }
        const auto im = palabras.find("mono");
        if (im != palabras.end()) {
          const std::string m = strLower(im->second);
          const std::string fijo = (m == "si" || m == "no") ? m : "auto";
          if (fijo != cfg.mono) { cfg.mono = fijo; elegida = true; }
        }
        if (elegida && !primerOrden) {
          Camera::Info info;
          const bool sabe = !cfg.camara.empty() && Camera::describe(cfg.camara, &info);
          std::string como = cfg.camara.empty() ? "la que encuentre el visor" : cfg.camara;
          if (sabe) {
            como += " (" + (info.nombre.empty() ? std::string("sin nombre") : info.nombre);
            if (!info.usb.empty()) como += " " + info.usb;
            if (!info.modos.empty()) {
              char dim[48];
              std::snprintf(dim, sizeof dim, ", %dx%d", info.modos.front().width, info.modos.front().height);
              como += dim;
            }
            como += info.esMonitor ? ", estereo del monitor)" : ", webcam aparte)";
          } else if (!cfg.camara.empty()) {
            como += " · ojo: no se pudo abrir)";
          }
          std::printf("[serve] camara del Facetracking ahora %s. Vale para la PROXIMA sesion: la actual"
                      " sigue con la que tenia abierta (%s)\n", como.c_str(),
                      cfg.rastreo ? "el visor la tiene cogida" : "sin rastreo pedido");
          std::fflush(stdout);
          ajuste::registrar("camara", antesCamara.empty() ? "-" : antesCamara,
                            cfg.camara.empty() ? "-" : cfg.camara, redActiva, "la ventana de ajustes");
          if (antesMono != cfg.mono)
            ajuste::registrar("mono", antesMono, cfg.mono, redActiva, "la ventana de ajustes");
        }
        primerOrden = false;
      }
    }

    /* Ningun navegador se queda a pantalla completa sin video. Medido el 24/11 a las 20:53: la
       sesion se cerro porque la ventana del video salio de pantalla completa y UN segundo despues
       Firefox se la volvio a poner el solo, de modo que el escritorio quedaba ocupado por una
       pagina a pantalla completa sin nada que ver. Solo lo hacemos con la ventana de una sesion que
       cerro PORQUE EL VIDEO SE FUE: cuando uno elige 2D plano, entrega la pantalla con Tab o saca el
       visor con ESC, el video sigue sonando y la pantalla completa es suya, ahi no se toca.
       Tampoco peleamos con el navegador: tres intentos separados 1,2 s dentro de los 6 segundos
       siguientes; si la vuelve a poner despues de eso, se avisa una sola vez y se lo deja a el.
       Se apaga con `devolver_fullscreen = no` en serve.conf. */
    if (hijo <= 0) {
      const std::string nombrePanel =
          !cfg.pantalla.empty() ? cfg.pantalla : (hayTarjeta && ov ? ov->outputName() : std::string());
      for (const WindowMonitor::Salida &sal : wm.outputs())
        if (sal.name == nombrePanel && sal.w > 0 && sal.h > 0) { reposoW = sal.w; reposoH = sal.h; }
    }
    if (cfg.devolverFull && !seFueUuid.empty()) {
      WindowInfo v{};
      const bool viva = wm.find(seFueUuid, v) && v.mapped;
      /* Ojo: al cerrar la sesion la ventana YA esta en su tamano normal (por eso la cerramos), y un
         segundo DESPUES la pagina se la vuelve a poner. Medido el 24/11 a las 20:53 con Firefox y
         vuelto a medir a las 22:04 con mpv: si uno se rinde al primer vistazo porque "ya esta normal",
         nunca llega a ver la reposicion de la pagina. Mientras dure la vigilia NO nos vamos porque este
         normal: esperamos a ver si se la vuelve a poner. */
      /* v0.73: la vigilia dura 6 s y caduca aunque la ventana este normal. Antes solo caducaba al
         verla a pantalla completa, o sea que se quedaba armada sin fin: medido el 25-nov, seis veces
         salto minutos despues ("sigue a pantalla completa despues de 0 intento(s)") cuando Rodolfo
         volvio a poner el video, y con menos de 20 s le habria quitado la pantalla completa que el
         acababa de pedir. La reposicion de Firefox que esto vigila llega al segundo del cierre. */
      const bool yaFuera = now - seFueDesde > 6.0 || fullQuitadas >= 3;
      if (!viva) {
        seFueUuid.clear();     /* la ventana cerro: ya no hay nada que devolver */
      } else if (!v.fullscreen()) {
        if (yaFuera) seFueUuid.clear();   /* se quedo normal: fin de la vigilia */
      } else if (yaFuera && fullQuitadas == 0) {
        seFueUuid.clear();     /* la puso el usuario pasada la vigilia: es suya, sin aviso */
      } else if (yaFuera) {
        if (!fullAvisado) {
          fullAvisado = true;
          std::printf("[ventana] %s sigue a pantalla completa despues de %d intento(s) y no insistimos"
                      " mas: la pagina se la sigue poniendo ella. En serve.conf puede apagarse con"
                      " devolver_fullscreen = no.\n", seFueUuid.c_str(), fullQuitadas);
          std::fflush(stdout);
        }
        seFueUuid.clear();
      } else if (WindowInfo vj{}; wm.find(seFueUuid, vj) && esJuego(vj.appId)) {
        std::printf("[ventana] %s es un juego: no se le toca la pantalla completa\n", vj.appId.c_str());
        std::fflush(stdout);
        seFueUuid.clear();
      } else if (now - fullDesde >= 1.2) {
        fullDesde = now;
        ++fullQuitadas;
        std::string e9;
        const bool pudo = wm.setState(seFueUuid, WindowInfo::Fullscreen, false, &e9);
        std::printf("[ventana] el video se fue y la ventana del navegador sigue a pantalla completa:"
                    " se la quitamos (%d/3)%s\n", fullQuitadas,
                    pudo ? "" : ("  El compositor dijo: " + e9).c_str());
        std::fflush(stdout);
      }
    }

    /* Si el gestor de ventanas minimiza la ventana del visor (Meta+F9, un clic en la barra de
       tareas, un gesto del touchpad) la trama desaparece, pero el monitor se queda en modo 3D y ya
       nadie puede mandar sobre la pastilla: la devolvemos nosotros. Al devolverla KWin no la
       activa (se queda DETRAS del video) y la coloca en la salida que le place: por eso pedimos
       tambien el "encima de todo", la pantalla completa y la salida del panel. La unica
       minimizacion que dejamos en paz es la nuestra, la de Tab, que el visor avisa escribiendo
       ~/.local/state/odysseyvk/entregada con su numero de proceso. */
    if (hijo > 0) {
      const std::string panelSalida =
          !cfg.pantalla.empty() ? cfg.pantalla : (hayTarjeta && ov ? ov->outputName() : std::string());
      WindowInfo visor{};
      bool hayVisor = false;
      for (const WindowInfo &w : wm.windows()) {
        if (w.appId != "odysseyvk" || w.title != NOMBRE_APP) continue;   /* el visor, no la tarjeta */
        visor = w;
        /* la barra de tareas y Alt+Tab (v1.02): se pone cada vez que no casa con serve.conf, asi un
           cambio en el panel vale al momento, tambien en la sesion en curso */
        {
          std::string e8;
          const bool fueraBarra = (w.states & WindowInfo::SkipTaskbar) != 0;
          if (fueraBarra == cfg.barraTareas) wm.setState(w.uuid, WindowInfo::SkipTaskbar, !cfg.barraTareas, &e8);
        }
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
            /* Con un juego a pantalla completa EXCLUSIVA esto pasa una y otra vez: KWin pone la
               ventana activa a pantalla completa en su capa de arriba, por encima incluso de las que
               llevan «encima fijo», y el visor no puede activarse (le quitaria el teclado y el raton
               al juego). MEDIDO con Cyberpunk y con gamescope (26-nov 00:22): el visor devuelto dos
               veces por sesion y el juego siempre delante. La salida es que el juego vaya en ventana
               sin bordes: se avisa con la tarjeta a la segunda vez. */
            if (esJuego(hijoApp) && ++visorEscondidas == 2)
              mostrarAviso([](std::string &t, std::vector<std::string> &l) {
                t = idioma::tr("El juego tapa el 3D");
                l = {idioma::tr("A pantalla completa exclusiva el escritorio pone al juego sobre el visor."),
                     idioma::tr("Ponlo en ventana sin bordes (borderless): así el 3D se ve encima."),
                     idioma::tr("Con gamescope igual: también tiene que ir sin bordes.")};
              }, now);
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
          /* Y en el otro sentido: si el video ya no esta donde debe, la trama deja de tener sentido
             encima de lo que sea que se vea ahora. El hub tarda unos 2,5 s en decidir el cierre (para
             no cerrar por un parpadeo), y en ese hueco se veian franjas sobre el escritorio: con la
             marca de oculto el visor deja de trazar al instante y, si el video vuelve, volvemos a
             poner "visible" y sigue como si nada. */
          escribirMarcaVentana(fueraVideoDesde > 0.0 ? "oculta" : "visible", now);
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
      /* El apilamiento es la otra mitad del "por encima", y era la que se rompia con un click: KWin
         sube al frente la ventana a pantalla completa que recibe el foco. Si el usuario toca el video
         (la barra de tiempos de YouTube) el navegador recupera el foco y tapa la trama, la pantalla se
         ve otra vez en 2D y "se empieza a ver mal". Medido con `ctl apilamiento` (de abajo hacia
         arriba): con el video activo el visor baja de 11 a 10 conservando su KeepAbove, y ninguna
         combinacion de KeepAbove / pantalla completa del visor lo vuelve a subir; este protocolo
         (plasma-window-management, rev 21) no tiene pedido de "elevar" ni de "activar", asi que no hay
         forma de traer el visor al frente. Lo que si funciona, medido: poner el VIDEO con KeepBelow.
         Ahí el video queda en la posicion 1 del apilamiento, el visor en la 11, y activar el video otra
         vez ya no lo sube por encima del visor. Al terminar la sesion se lo sacamos. */
      if (hayVisor && !cerrandoSesion) {
        WindowInfo foco{};
        const bool videoVivo = wm.find(hijoUuid, foco) && foco.mapped;
        if (videoVivo && !(foco.states & WindowInfo::KeepBelow) && now - debajoDesde >= 1.0) {
          debajoDesde = now;
          std::string e5;
          if (!wm.setState(hijoUuid, WindowInfo::KeepBelow, true, &e5) && !avisoPilaHecho) {
            avisoPilaHecho = true;
            std::printf("[ventana] el compositor no quiso dejar el video por debajo del visor: %s\n",
                        e5.c_str());
            std::fflush(stdout);
          } else if (!debajoAvisado) {
            debajoAvisado = true;
            std::printf("[ventana] el video pasa por debajo del visor: un click en el video (la barra de"
                        " tiempos) ya no tapa la trama\n");
            std::fflush(stdout);
          }
        }
        /* Las demas ventanas, tambien por debajo: si no, el click que el visor deja pasar cae en la
           ventana que este arriba en ese punto y el video no se entera. Cada cinco segundos, por si
           abrieron algo durante la sesion. */
        if (videoVivo && (foco.states & WindowInfo::KeepBelow) && now - otrosDesde >= 5.0) {
          otrosDesde = now;
          const int hechas = bajarDemas();
          if (hechas > 0 && !otrosAvisado) {
            otrosAvisado = true;
            std::printf("[ventana] %d ventana(s) mas pasaron por debajo del visor: asi el click que el"
                        " visor deja pasar llega al video y no a otra ventana. Las devolvemos al"
                        " terminar la sesion.\n", hechas);
            std::fflush(stdout);
          }
        }
        /* La red: si a pesar de eso el compositor puso el video por encima del visor, lo volvemos a
           meter debajo (quitarla y ponerla: reenviar el mismo estado no hace nada) y revisamos. */
        const bool focoMovido = videoVivo && foco.active() && !videoActivaAntes;
        videoActivaAntes = videoVivo && foco.active();
        if ((focoMovido || now - pilaDesde >= 5.0) && now - pilaDesde >= 0.5) {
          pilaDesde = now;
          std::vector<std::string> pila;
          std::string e5;
          if (wm.stackingOrder(&pila, &e5)) {
            int lugarVisor = -1, lugarVideo = -1;
            for (size_t i = 0; i < pila.size(); ++i) {
              if (pila[i] == visor.uuid) lugarVisor = (int)i + 1;
              else if (pila[i] == hijoUuid) lugarVideo = (int)i + 1;
            }
            if (lugarVisor > 0 && lugarVideo > 0 && lugarVisor < lugarVideo) {
              wm.setState(hijoUuid, WindowInfo::KeepBelow, false, &e5);
              ++apilados;
              if (!wm.setState(hijoUuid, WindowInfo::KeepBelow, true, &e5)) {
                if (!avisoPilaHecho) {
                  avisoPilaHecho = true;
                  std::printf("[ventana] el video tapa la trama y el compositor no quiso meterlo debajo:"
                              " %s\n", e5.c_str());
                  std::fflush(stdout);
                }
              } else if (apilados <= 3 || apilados % 20 == 0) {
                std::printf("[ventana] el video se habia puesto encima del visor (visor en %d de %zu,"
                            " video en %d): lo volvemos a meter debajo\n", lugarVisor, pila.size(),
                            lugarVideo);
                std::fflush(stdout);
              }
            }
          } else if (!avisoPilaHecho) {
            avisoPilaHecho = true;
            std::printf("[ventana] %s\n", e5.c_str());
            std::fflush(stdout);
          }
        }
      }
      /* El otro lado de la trama es el video, y la pantalla completa del video se pierde por dos
         motivos que hay que tratar distinto:
         - El cambio de modo del panel. KWin suelta la pantalla completa de la ventana del video y la
           deja achicada y corrida. Medido ventana por segundo con `ctl windows`: en 2D esta en 0,0
           2560x1440, y al arrancar la sesion pasa a 640,360 sin el estado. En la sesion real el flujo
           seguia llegando a 2560x1440 mientras el panel estaba en 3840x2160, o sea el video no tapaba
           el lienzo y la trama se veia mal. Eso lo provoca nuestro propio cambio de modo, asi que se
           lo devolvemos al video.
         - El usuario, que se va del video (ESC, cambiar de pestana, minimizar, cerrar la ventana). Ahi
           la ventana se queda sin pantalla completa y no vuelve: la salida del video es el boton de
           apagar, y mandamos SIGTERM al visor para que se vaya por la puerta de adelante (devuelve el
           panel a 2D, apaga lente y scaler). Antes nos peleabamos con esto y la sesion no se cerraba.
         Lo que las separa es el reloj: mientras la sesion es joven (los segundos del cambalache del
         cambio de modo) insistimos con la pantalla completa; asentada la sesion, si al video le falta
         el estado o no cubre la salida del panel, cerramos la sesion. */
      if (!hijoUuid.empty()) {
        WindowInfo video{};
        const char *motivo = nullptr;
        double paciencia = 1.5;   /* lo que tiene que durar la senal antes de cerrar */
        const bool asentada = hijoDesde > 0.0 && now - hijoDesde >= 12.0;
        /* La pantalla completa solo se repone mientras dura nuestro cambio de modo. v0.73, medido el
           25-nov en 20 sesiones: lo que el cambio de modo le hace a la ventana del video termina a
           los 2 s de lanzar la sesion, y ninguna vez le quito el estado. La unica reposicion del dia
           (09:04:19, sesion de 11 s) fue Rodolfo saliendo del video: se la volvimos a poner y el
           navegador quedo a pantalla completa sin video, con la trama encima otros 7 s. Pasados 5 s,
           perder el estado es una salida del video y cierra la sesion. */
        const bool enCambioDeModo = hijoDesde > 0.0 && now - hijoDesde < 5.0;
        int sx = 0, sy = 0, sw = 0, sh = 0;
        bool salidaVista = false;
        for (const WindowMonitor::Salida &s : wm.outputs()) {
          if (s.name != panelSalida || s.w <= 0 || s.h <= 0) continue;
          sx = s.x; sy = s.y; sw = s.w; sh = s.h; salidaVista = true;
          break;
        }
        if (vueltaPendiente && cerrandoSesion && salidaVista && sw == reposoW && sh == reposoH) {
          /* el monitor ya esta como siempre: la pantalla completa que dejamos puesta al cerrar sobra.
             No se espera a que el visor termine de morir (medio segundo mas de ventana a pantalla
             completa a la vista). */
          vueltaPendiente = false;
          seFueUuid = hijoUuid;
          seFueDesde = now;
          fullDesde = 0.0;
          fullQuitadas = 0;
          fullAvisado = false;
        }
        if (!wm.find(hijoUuid, video) || !video.mapped) motivo = "la ventana del video ya no esta";
        else if (video.minimized()) {
          if (videoMinimizadaDesde == 0.0) videoMinimizadaDesde = now;
          if (!esJuego(video.appId) || now - videoMinimizadaDesde >= 6.0) motivo = "la ventana del video esta minimizada";
        } else {
          videoMinimizadaDesde = 0.0;
          const std::string firma = WindowInfo::stateList(video.states) + " " +
                                    std::to_string((int)video.x) + "," + std::to_string((int)video.y) +
                                    " " + std::to_string((int)video.w) + "x" + std::to_string((int)video.h);
          if (!firmaVideo.empty() && firma != firmaVideo) {
            /* cada cambio de la ventana del video queda apuntado con su reloj: asi se ve quien lo
               provoco (nuestro cambio de modo, el compositor, o el propio reproductor) */
            std::printf("[ventana] video: %s -> %s  (sesion de %.0f s)%s\n", firmaVideo.c_str(),
                        firma.c_str(), hijoDesde > 0.0 ? now - hijoDesde : 0.0,
                        asentada ? "" : ", sesion joven");
            std::fflush(stdout);
          }
          firmaVideo = firma;
          const bool sinEstado = !video.fullscreen();
          /* el 3D cambio el tamano logico del panel: solo entonces KWin puede haber movido la ventana.
             Sin ese cambio (v0.76), perder la pantalla completa es siempre el usuario saliendo. */
          const bool cambioPanel = reposoW > 0 && salidaVista && (sw != reposoW || sh != reposoH);
          /* "chica" = la ventana conserva el tamano que tenia la salida en 2D. Es el caso que mas se ve
             tras nuestro cambio de modo: el flujo llegaba a 2560x1440 con el panel en 3840x2160. */
          const bool chica = salidaVista && ((int)video.w < sw || (int)video.h < sh);
          /* NO le quitamos la pantalla completa al video para agrandarlo. Antes, si el video tenia el
             estado puesto pero todavia media lo que media la salida en 2D, la quitabamos y la volvia-
             mos a poner para que KWin la estirara otra vez. Medido el 03:38 con el navegador: ese
             va-y-ven hace que el navegador SALGA del video a pantalla completa, o sea la pagina vuelve
             al reproductor chico dentro de la pestana y la trama se ve sobre la pagina entera. El aviso
             viejo lo confirma: "(2560x1440 en una salida de 3840x2160, pantalla completa si): lo
             reacomodamos (1/8)" justo antes de que se viera asi.
             Con el estado puesto no hay que tocar nada: KWin ya la tiene a pantalla completa y, tras
             nuestro cambio de modo, el tamano nuevo llega solo (medido: el flujo del navegador paso de
             2560x1440 a 3840x2160 a los siete segundos de la sesion). Mientras tanto la trama se
             estira: se ve algo menos nitido, pero no roto y no rompe el video. */
          /* Con un NAVEGADOR no se le repone la pantalla completa (v0.75). Cuando KWin se la quita
             al cambiar el monitor, el navegador ya solto el video (volvio a su hueco en la pagina),
             y si se la ponemos nosotros queda en SU pantalla completa, la de F11: la trama se teje
             sobre la pagina, hay que volver a pulsar el boton del video y luego ESC ya no saca de
             ahi (lo que conto Rodolfo el 25-nov: «al salir del video se quedaba la ventana en el
             modo 3D»). Se cierra la sesion limpia, se avisa y al volver a poner el video a pantalla
             completa arranca bien, porque al cerrar dejamos la ventana como KWin tiene que
             recordarla (ver mas abajo). Si a los tres minutos vuelve a pasar con la misma ventana,
             es que eso no basto: ahi si se repone, como antes, para no dejar al usuario sin 3D. */
          static const char *kNavegadores[] = {"firefox", "zen", "chrom", "brave", "epiphany", "midori",
                                               "edge", "vivaldi", "opera", "librewolf", "floorp"};
          bool esNavegadorVideo = false;
          for (const char *nav : kNavegadores)
            if (strLower(video.appId).find(nav) != std::string::npos) esNavegadorVideo = true;
          const bool yaPaso = caidaUuid == hijoUuid && now - caidaHora < 180.0;
          if (enCambioDeModo && cambioPanel && !cerrandoSesion && sinEstado && esNavegadorVideo && !yaPaso && reafirmadas == 0) {
            caidaUuid = hijoUuid;
            caidaHora = now;
            motivo = "el escritorio saco al video de pantalla completa al cambiar el monitor";
            videoSeFue = true;
            paciencia = 0.0;
            std::printf("[ventana] %s: con un navegador no se la reponemos (quedaria en la pantalla completa"
                        " de F11, con la trama sobre la pagina). Cerramos y avisamos: al volver a poner el"
                        " video a pantalla completa arranca bien\n", motivo);
            std::fflush(stdout);
            static Notifier avisoCaida;
            static bool avisoCaidaAbierto = false;
            if (!avisoCaidaAbierto) avisoCaidaAbierto = avisoCaida.open(nullptr);
            if (avisoCaidaAbierto)
              avisoCaida.send("odysseyvk-caida", "El 3D no llegó a empezar",
                              "Al cambiar el monitor, el escritorio sacó el video de pantalla completa."
                              " Vuelve a ponerlo a pantalla completa y ya entra.", {}, nullptr);
          } else if (enCambioDeModo && cambioPanel && !cerrandoSesion && sinEstado && reafirmadas < 3 && now - reacomodoDesde >= 0.6) {
            reacomodoDesde = now;
            ++reafirmadas;
            std::string e4;
            const bool pudo = wm.setState(hijoUuid, WindowInfo::Fullscreen, true, &e4);
            if (reafirmadas == 1 || reafirmadas == 3 || !pudo)
              std::printf("[ventana] el cambio de modo dejo al video sin pantalla completa (%ux%u en una"
                          " salida de %dx%d): se la devolvemos (%d/3)%s. Si el video volvio a su"
                          " reproductor dentro de la pagina, volvé a ponerlo a pantalla completa.\n",
                          (unsigned)video.w, (unsigned)video.h, sw, sh, reafirmadas,
                          pudo ? "" : ("  El compositor dijo: " + e4).c_str());
            std::fflush(stdout);
            fueraVideoDesde = 0.0;   /* esto lo provocamos nosotros, no es una salida del video */
          } else if (!asentada && chica && now - hijoDesde >= 2.5 && !avisoChicoHecho) {
            avisoChicoHecho = true;  /* avisar una vez: la ventana no se toca */
            std::printf("[ventana] el video todavia esta a %ux%u en una salida de %dx%d despues del cambio"
                        " de modo: la trama se estira un poco mientras la ventana vuelve a llenar la"
                        " salida. No le tocamos la pantalla completa, porque eso saca al video de su"
                        " propio modo a pantalla completa.%s\n",
                        (unsigned)video.w, (unsigned)video.h, sw, sh,
                        " Ctrl+Shift+1 sale del 3D si lo queres ver.");
            std::fflush(stdout);
          } else if ((!enCambioDeModo || !cambioPanel) && sinEstado) {
            motivo = "la ventana del video salio de pantalla completa";
            videoSeFue = true;   /* el video se fue el solo: la ventana no tiene por que seguir a pantalla completa */
            /* Menos espera que antes: la trama ya se quita al instante con la marca de oculto, y
               mientras la sesion sigue abierta el panel esta en modo 3D sobre un escritorio 2D.
               v0.63: 1,2 -> 0,4 s. Medido el 25-nov 07:24: Firefox salio de pantalla completa a las
               07,09 y el panel no volvio a 2D hasta las 08,77, y 1,2 s de eso era esta espera. Las
               salidas falsas que la pedian son las del arranque (nuestro cambio de modo), y esta rama
               solo corre con la sesion ya asentada; 0,4 s son dos vueltas del bucle (200 ms). */
            paciencia = 0.4;
          } else if (asentada && chica && !avisoChicoHecho) {
            avisoChicoHecho = true;   /* ya no insistimos: avisamos una vez y dejamos mirar */
            std::printf("[ventana] el video sigue a %ux%u en una salida de %dx%d: la trama no va a ser"
                        " pixel a pixel. Volvé a poner pantalla completa en la aplicación.\n",
                        (unsigned)video.w, (unsigned)video.h, sw, sh);
            std::fflush(stdout);
          } else if (asentada && salidaVista) {
            const int cx = video.x + (int)video.w / 2, cy = video.y + (int)video.h / 2;
            if (cx < sx || cx >= sx + sw || cy < sy || cy >= sy + sh)
              motivo = "la ventana del video se fue de la salida del panel";
            paciencia = 1.2;
          }
        }
        if (motivo) {
          if (fueraVideoDesde <= 0.0) fueraVideoDesde = now;
          if (!cerrandoSesion && now - fueraVideoDesde >= paciencia && hijo > 0) {
            cerrandoSesion = true;
            std::printf("[ventana] %s (%s): cerramos la sesion 3D y devolvemos el panel a 2D\n",
                        motivo, hijoUuid.c_str());
            std::fflush(stdout);
            /* un juego cuya ventana desaparece a los pocos minutos de entrar en 3D: lo mas probable es
               que se haya caido (v1.00). Se avisa con lo que suele arreglarlo. */
            if (esJuego(hijoApp) && std::string(motivo) == "la ventana del video ya no esta" && hijoDesde > 0.0 && now - hijoDesde < 300.0) {
              const std::string segundos = std::to_string((long)std::lround(now - hijoDesde));
              mostrarAviso([segundos](std::string &t, std::vector<std::string> &l) {
                t = idioma::trf("El juego se cerró a los {} s de entrar en 3D", {segundos});
                l = {idioma::tr("Si se cayó, ábrelo en ventana sin bordes (borderless) y vuelve a probar."),
                     idioma::tr("A pantalla completa exclusiva choca con el visor al cambiar de modo."),
                     idioma::tr("Para dejarlo en 2D, cambia su interruptor en la pestaña Juegos.")};
              }, now);
            }
            /* KWin guarda, para cada configuracion de pantallas, como estaba cada ventana, y al
               volver a esa configuracion se lo repone (v0.75). Medido el 25-nov: a las 11:25:32,
               en el mismo segundo en que el visor devolvio el monitor a su escala, la ventana del
               video paso sola de «maximizada» a «pantalla completa» (lo que tenia al empezar la
               sesion); y las tres sesiones siguientes EMPEZARON con el video sacado de pantalla
               completa (11:14:49, 11:17:28, 11:29:32), porque la ultima vez que el monitor estuvo
               en la configuracion de 3D la ventana ya no lo estaba: el usuario acababa de salir.
               Con el navegador eso es grave: al quitarle la pantalla completa suelta el video, y
               si se la volvemos a poner nosotros queda en SU pantalla completa (la de F11), de la
               que ESC ya no sale, con la trama encima.
               Por eso, antes de devolver el monitor, la ventana vuelve a quedar a pantalla completa
               un instante: asi es como KWin la recuerda para la configuracion de 3D y la proxima
               sesion no arranca sacando al video. Al llegar a la configuracion de siempre se le
               quita (la vigilia de abajo). Solo si el 3D cambio de verdad el tamano logico del
               panel: con el escritorio ya a escala 1 no hay cambio de configuracion ni nada que
               recordar. */
            if (videoSeFue && cfg.devolverFull && reposoW > 0 && salidaVista && (sw != reposoW || sh != reposoH)) {
              std::string e7;
              if (wm.setState(hijoUuid, WindowInfo::Fullscreen, true, &e7)) {
                wm.flush();
                wm.waitAndDispatch(80);
                vueltaPendiente = true;
                std::printf("[ventana] antes de devolver el monitor la ventana del video queda a pantalla"
                            " completa un instante (asi la recuerda el escritorio para la proxima sesion);"
                            " se le quita al volver\n");
              } else {
                std::printf("[ventana] no se pudo dejar la ventana a pantalla completa antes de devolver el"
                            " monitor: %s\n", e7.c_str());
              }
              std::fflush(stdout);
            }
            ::kill(hijo, SIGTERM);
          }
        } else {
          fueraVideoDesde = 0.0;
        }
      }
    } else {
      borrarMarcaVentana();
    }

    /* El navegador dice por MPRIS si el video de la pagina esta Reproduciendo, Pausado o Parado, y
       esa es la unica senal que falta para quitar la trama al salir del video. Hace falta porque en
       YouTube la pantalla completa la sostiene la pagina (el video es un elemento de la pagina), de
       modo que al irse del video la ventana del navegador sigue a pantalla completa y por la ventana
       no hay forma de enterarse: medido en la sesion real, la trama se quedaba tejida sobre el
       navegador hasta que la pagina salia de pantalla completa.
       Reglas: solo con ventanas de navegador y con el reproductor MPRIS del propio navegador (asi no
       cierra la sesion el "Parado" de un reproductor de musica abierto). "Pausado" no cierra nada,
       porque una pausa es una pausa y ahi la trama se queda viendo. "Parado" durante video_parado
       segundos, o el reproductor que desaparece del bus (la pagina se quedo sin video), cierra la
       sesion por la puerta de adelante: SIGTERM al visor, que devuelve el panel a 2D y apaga la lente. */
    if (hijo > 0 && !cerrandoSesion && cfg.paradoSeg > 0 && hijoDesde > 0.0 && now - hijoDesde >= 3.0) {
      static const char *navegadores[] = {"firefox", "zen", "chrom", "brave", "epiphany", "midori",
                                          "edge", "vivaldi", "opera"};
      auto esNavegador = [](const char *s) {
        if (!s || !*s) return false;
        for (const char *p : navegadores)
          if (std::strstr(s, p)) return true;
        return false;
      };
      WindowInfo ventanaVideo{};
      if (wm.find(hijoUuid, ventanaVideo) && esNavegador(ventanaVideo.appId.c_str())) {
        std::string errM, estado;
        if (!navegadorAbierto) {
          navegador.pista(ventanaVideo.appId);   /* que gane el reproductor de ESA ventana (v0.60) */
          navegadorAbierto = navegador.open(&errM) && esNavegador(navegador.playerName().c_str());
          if (!navegadorAbierto && !navegadorAvisado) {
            navegadorAvisado = true;
            std::printf("[video] %s: no vemos el estado del video por MPRIS (%s), asi que la trama se quita"
                        " solo cuando la pagina salga de pantalla completa\n",
                        ventanaVideo.appId.c_str(), errM.empty() ? "el reproductor que aparece no es del navegador"
                                                                : errM.c_str());
            std::fflush(stdout);
          }
        }
        if (navegadorAbierto) {
          bool parado = false;
          const bool leido = navegador.status(&estado, &errM);
          if (leido && !estado.empty()) {
            parado = (estado == "Stopped");
            if (estado != estadoNavegador) {
              std::printf("[video] %s dice %s (sesion de %.0f s)\n", navegador.playerName().c_str(),
                          estado.c_str(), now - hijoDesde);
              std::fflush(stdout);
              estadoNavegador = estado;
            }
          } else if (!estadoNavegador.empty()) {
            parado = true;   /* el reproductor desaparecio del bus: la pagina se quedo sin video */
            estado = "sin reproductor";
          }
          if (parado) {
            if (paradoDesde <= 0.0) {
              paradoDesde = now;
              std::printf("[video] %s: puede ser un cambio de pestana; si dura %d s quitamos la trama\n",
                          estado.c_str(), cfg.paradoSeg);
              std::fflush(stdout);
            }
            if (now - paradoDesde >= (double)cfg.paradoSeg) {
              cerrandoSesion = true;
              videoSeFue = true;   /* el navegador dice que no hay video: la pantalla completa le sobra */
              std::printf("[ventana] el navegador dice %s: el video se fue, cerramos la sesion 3D y"
                          " devolvemos el panel a 2D (%s)\n", estado.c_str(), hijoUuid.c_str());
              std::fflush(stdout);
              ::kill(hijo, SIGTERM);
            }
          } else {
            paradoDesde = 0.0;
          }
        }
      }
    }

    /* el icono de la bandeja: puesto o quitado segun serve.conf; si la bandeja no lo acepta (aun no
       arranco el panel de KDE), se reintenta cada medio minuto */
    if (!dry && cfg.bandeja && !bandeja.puesta() && now - bandejaIntento >= 30.0) {
      bandejaIntento = now;
      std::string eb;
      if (bandeja.poner(&eb)) std::printf("[bandeja] icono puesto en la bandeja del sistema\n");
      else std::printf("[bandeja] %s (se reintenta)\n", eb.c_str());
      std::fflush(stdout);
    } else if (!cfg.bandeja && bandeja.puesta()) {
      bandeja.quitar();
      std::printf("[bandeja] icono quitado\n");
      std::fflush(stdout);
    }

    /* La actualizacion (v0.95): mirar GitHub de vez en cuando y, con una version nueva, la tarjeta */
    if (proximaComprobacion == 0.0) proximaComprobacion = now + 30.0;
    if (cfg.actualizaciones != "no" && now >= proximaComprobacion && !prefs::comprobacionEnMarcha()) {
      proximaComprobacion = now + cfg.actualizacionesCada * 3600.0;
      /* con la version de GitHub igual a la instalada, la siguiente mirada antes (una hora como
         mucho): asi una version recien subida no espera seis horas a verse (v1.01) */
      if (prefs::estadoActualizacion().alDia) proximaComprobacion = now + std::min(cfg.actualizacionesCada, 1.0) * 3600.0;
      prefs::comprobarActualizacion(nullptr);
    }
    if (prefs::comprobacionEnMarcha() && prefs::vigilarComprobacion()) {
      const prefs::Actualizacion a = prefs::estadoActualizacion();
      std::printf("[serve] version en GitHub: %s (instalada %s)%s\n", a.remota.empty() ? "no se pudo mirar" : a.remota.c_str(),
                  ODYSSEYVK_VERSION, a.pendiente ? " · hay una actualizacion" : "");
      std::fflush(stdout);
      const bool sola = a.pendiente && !dry && (cfg.actualizarAuto == "si" || cfg.actualizarAuto == "1") && versionAvisada != a.remota;
      if (sola) {
        /* actualizar_auto: sin tarjeta, se instala ya (la clave la pide pkexec en una ventana) */
        versionAvisada = a.remota;
        if (FILE *f = std::fopen(rutaEstadoOdyssey("actualizacion-avisada").c_str(), "we")) {
          std::fprintf(f, "%s\n", a.remota.c_str());
          std::fclose(f);
        }
        std::string e;
        std::printf("[serve] actualizacion automatica a %s: %s\n", a.remota.c_str(),
                    prefs::lanzarActualizacion(&e) ? "arrancada aparte" : e.c_str());
        std::fflush(stdout);
      } else if (a.pendiente && !dry && cfg.actualizaciones != "no" && hayTarjeta && !ov->askVisible() && versionAvisada != a.remota) {
        versionAvisada = a.remota;
        if (FILE *f = std::fopen(rutaEstadoOdyssey("actualizacion-avisada").c_str(), "we")) {
          std::fprintf(f, "%s\n", a.remota.c_str());
          std::fclose(f);
        }
        ov->hud().state().askActualizacion = a.remota;
        ov->hud().state().version = std::string("OdysseyVK ") + ODYSSEYVK_VERSION;
        ov->showAsk(std::string(), std::string());
        tarjetaActualizacion = true;
        tarjetaActT = now;
        std::printf("[serve] tarjeta de actualizacion puesta (%s)\n", a.remota.c_str());
        std::fflush(stdout);
      }
    }
    if (tarjetaAviso) {
      std::string acto = ov->takeAction();
      if (acto.empty() && now - tarjetaAvisoT > 60.0) acto = "luego";
      if (!acto.empty()) {
        ov->hideAsk();
        ov->hud().state().askAviso.clear();
        ov->hud().state().askAvisoLineas.clear();
        tarjetaAviso = false;
      }
    }
    if (tarjetaActualizacion) {
      std::string acto = ov->takeAction();
      if (acto.empty() && now - tarjetaActT > 180.0) acto = "luego";
      if (!acto.empty()) {
        ov->hideAsk();
        ov->hud().state().askActualizacion.clear();
        tarjetaActualizacion = false;
        std::string e;
        if (acto == "actualizar") {
          std::printf("[serve] tarjeta -> actualizar: %s\n", prefs::lanzarActualizacion(&e) ? "arrancada aparte" : e.c_str());
        } else if (acto == "noavisar") {
          cfg.actualizaciones = "no";
          if (!ajuste::fijarEnServeConf("actualizaciones", "no", &e)) std::fprintf(stderr, "[serve] %s\n", e.c_str());
          std::printf("[serve] tarjeta -> no avisar mas de actualizaciones (el boton junto al aspa las sigue marcando)\n");
        } else {
          std::printf("[serve] tarjeta de actualizacion cerrada (%s)\n", acto.c_str());
        }
        std::fflush(stdout);
      }
    }

    /* Los ajustes del panel (`odysseyvk ajustes`) se escriben en serve.conf. Antes hacia falta
       reiniciar el servicio para que valieran; ahora, cuando el fichero cambia, se releen los que el
       panel toca. Los demas (camara, pantalla, numeros de la trama) siguen siendo de arranque. */
    {
      static long long marcaConf = -2;
      static double ultimaMirada = 0.0;
      if (now - ultimaMirada >= 1.0) {
        ultimaMirada = now;
        struct stat sb {};
        const long long marca = ::stat(ajuste::rutaServeConf().c_str(), &sb) == 0
                                    ? (long long)sb.st_mtim.tv_sec * 1000000000LL + (long long)sb.st_mtim.tv_nsec + (long long)sb.st_size
                                    : -1;
        if (marcaConf == -2) marcaConf = marca;   /* la primera vez solo se apunta */
        /* sin ninguna red al arrancar: en cuanto la abierta aparece en su carpeta (el boton de descarga
           del panel), se toma para el siguiente visor (v0.94) */
        if (redActiva.empty() && fileExists(rutaRedNuestra())) elegirRedes();
        if (marca != marcaConf) {
          marcaConf = marca;
          ServeCfg nuevo;
          loadServeCfg(nuevo);
          const bool cambio = nuevo.modo != cfg.modo || nuevo.rastreo != cfg.rastreo || nuevo.hdrEn3d != cfg.hdrEn3d ||
                              nuevo.atajosGlobales != cfg.atajosGlobales || nuevo.crosstalkBase != cfg.crosstalkBase ||
                              nuevo.relieve != cfg.relieve || nuevo.divididoSuave != cfg.divididoSuave ||
                              nuevo.divididoAuto != cfg.divididoAuto ||
                              nuevo.porApp != cfg.porApp || nuevo.modelo != cfg.modelo ||
                              nuevo.actualizaciones != cfg.actualizaciones || nuevo.actualizarAuto != cfg.actualizarAuto;
          /* el idioma (v1.07): la tarjeta de la orilla y el icono de la bandeja pasan al nuevo ya; el
             atlas de letras de la capa se rehace con las del idioma (las chinas y japonesas aparte) */
          if (const int l = idioma::leerDeConfig(); !std::getenv("ODYSSEYVK_IDIOMA") && (l < 0 ? idioma::EN : l) != idioma::actual()) {
            idioma::poner(l < 0 ? idioma::EN : l);
            if (ov) {
              std::string e2;
              if (!ov->hud().buildAtlas(&e2)) std::fprintf(stderr, "[serve] atlas: %s\n", e2.c_str());
              if (tarjetaAviso && avisoTexto) avisoTexto(ov->hud().state().askAviso, ov->hud().state().askAvisoLineas);
              if (ov->askVisible()) ov->repaint();
            }
            std::printf("[serve] idioma: %s\n", idioma::codigo(idioma::actual()));
            std::fflush(stdout);
          }
          cfg.actualizaciones = nuevo.actualizaciones;
          cfg.barraTareas = nuevo.barraTareas;
          if (nuevo.bandeja && !cfg.bandeja) bandejaIntento = -100.0;   /* encendida en el panel: ponerla ya, sin esperar al reintento */
          cfg.bandeja = nuevo.bandeja;
          cfg.actualizarAuto = nuevo.actualizarAuto;
          if (nuevo.actualizacionesCada != cfg.actualizacionesCada) {
            cfg.actualizacionesCada = nuevo.actualizacionesCada;
            proximaComprobacion = now + 5.0;   /* con el intervalo nuevo, una mirada ya */
          }
          if (nuevo.modelo != cfg.modelo) {
            cfg.modelo = nuevo.modelo;
            elegirRedes();
          }
          cfg.modo = nuevo.modo;
          cfg.rastreo = nuevo.rastreo;
          cfg.hdrEn3d = nuevo.hdrEn3d;
          cfg.atajosGlobales = nuevo.atajosGlobales;
          /* la tarjeta de la red (`red = 1`): vale desde el siguiente visor, sin reiniciar el servicio */
          if (nuevo.red != cfg.red) {
            std::printf("[serve] la red de profundidad pasa a la tarjeta %d desde el siguiente video\n", nuevo.red);
            cfg.red = nuevo.red;
          }
          cfg.expRed = nuevo.expRed, cfg.expMapa = nuevo.expMapa, cfg.expTope = nuevo.expTope;
          cfg.expAnticipacion = nuevo.expAnticipacion;
          cfg.expPrioridad = nuevo.expPrioridad;
          cfg.crosstalkBase = nuevo.crosstalkBase;
          cfg.relieve = nuevo.relieve;
          cfg.divididoSuave = nuevo.divididoSuave;
          cfg.divididoAuto = nuevo.divididoAuto;
          cfg.porApp = nuevo.porApp;
          if (cambio) {
            std::printf("[serve] serve.conf cambio: modo=%s · rastreo %s · dividido %s · %d aplicacion(es) sin preguntar\n",
                        cfg.modo.c_str(), cfg.rastreo ? "si" : "no", cfg.divididoAuto ? "suavizado si hace falta" : (cfg.divididoSuave ? "suavizado" : "tal cual"),
                        (int)cfg.porApp.size());
            std::fflush(stdout);
          }
        }
      }
    }

    /* La casilla de «no volver a preguntar» de la pastilla del visor deja aqui su recado: se
       aplica a la memoria del servicio y a serve.conf. Desmarcada, la aplicacion vuelve a pasar por
       la tarjeta la proxima vez que se ponga a pantalla completa. */
    {
      std::string rApp, rModo;
      if (ajuste::tomarRecordar(rApp, rModo)) {
        if (rModo.empty()) cfg.porApp.erase(rApp);
        else cfg.porApp[rApp] = rModo;
        std::string e;
        if (!ajuste::fijarAutoEnServeConf(rApp, rModo, &e))
          std::fprintf(stderr, "[serve] no se pudo cambiar serve.conf: %s\n", e.c_str());
        if (rModo.empty())
          std::printf("[serve] desde la pastilla: se vuelve a preguntar por %s\n", rApp.c_str());
        else
          std::printf("[serve] desde la pastilla: %s pasa directo a %s sin preguntar\n", rApp.c_str(), rModo.c_str());
        std::fflush(stdout);
      }
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

    if (hayTarjeta && !tarjetaActualizacion && !tarjetaAviso) {
      if (std::string acto = ov->takeAction(); !acto.empty()) {
        ov->hideAsk();
        if (acto == "close") {
          std::printf("[serve] tarjeta cerrada sin elegir (%s)\n", tarjetaUuid.c_str());
        } else {
          accion = acto;
          avisoUuid = tarjetaUuid;
          accionApp = tarjetaApp;
          accionGuardada = ov->remember() && !tarjetaApp.empty() && acto != "flat";
          std::printf("[serve] tarjeta -> %s (%s)\n", acto.c_str(), avisoUuid.c_str());
          if (ov->remember() && !tarjetaApp.empty() && acto != "flat") {
            cfg.porApp[tarjetaApp] = acto;
            if (std::string e; !ajuste::fijarAutoEnServeConf(tarjetaApp, acto, &e))
              std::fprintf(stderr, "[serve] no se pudo apuntar en serve.conf: %s\n", e.c_str());
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
          hijo = spawnViewer(exe, avisoUuid, act, cfg.red, cfg.cada, redActiva, cfg.pantalla, redOtra, cfg.rastreo, cfg.suavizado, &cfg,
                             false, accionApp.empty() ? std::string() : accionApp + ":" + act, accionGuardada);
          accionApp.clear();
          accionGuardada = false;
          hijoDesde = now;
          hijoUuid = avisoUuid;
          hijoApp = accionApp;
          visorEscondidas = 0;
          std::printf("[serve] lanzando 3D (%s) para %s\n", act.c_str(), hijoUuid.c_str());
          std::fflush(stdout);
          if (hijo > 0 && unaVez) break;
        }
      }
      avisoUuid.clear();
    }

    /* desde cuando lleva cada ventana a pantalla completa (para no lanzar el 3D a un juego que aun se esta abriendo) */
    {
      std::set<std::string> aun;
      for (const WindowInfo &w : wm.windows())
        if (w.usable()) {
          aun.insert(w.uuid);
          if (!fullVisto.count(w.uuid)) fullVisto[w.uuid] = now;
        }
      for (auto it = fullVisto.begin(); it != fullVisto.end();)
        it = aun.count(it->first) ? std::next(it) : fullVisto.erase(it);
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
      if (wm.fullscreenCandidate(w, {}) && !ignoredApp(w, cfg.ignore) && !preguntadas.count(w.uuid) &&
          now >= enfriadoHasta && (!esJuego(w.appId) || (fullVisto.count(w.uuid) && now - fullVisto[w.uuid] >= 4.0))) {
        preguntadas.insert(w.uuid);
        const std::string app = strLower(w.appId);
        if (tarjetaActualizacion || tarjetaAviso) {   /* la pregunta del video manda sobre los avisos */
          ov->hideAsk();
          ov->hud().state().askActualizacion.clear();
          ov->hud().state().askAviso.clear();
          tarjetaActualizacion = false;
          tarjetaAviso = false;
        }
        auto fija = cfg.porApp.find(app);
        if (!reabrirUuid.empty() && w.uuid == reabrirUuid && !dry) {
          /* la sesion que corto la actualizacion: se reabre tal cual estaba, sin preguntar */
          hijo = spawnViewer(exe, w.uuid, reabrirModo, cfg.red, cfg.cada, redActiva, cfg.pantalla, redOtra, cfg.rastreo, cfg.suavizado, &cfg);
          hijoDesde = now;
          hijoUuid = w.uuid;
          hijoApp = strLower(w.appId);
          visorEscondidas = 0;
          std::printf("[serve] reabierto en %s tras la actualizacion (%s)\n", reabrirModo.c_str(), app.c_str());
          std::fflush(stdout);
          reabrirUuid.clear();
        } else if (fija != cfg.porApp.end() && fija->second == "flat") {
          std::printf("[serve] %s esta marcada como 2D plano: no se hace nada\n", app.c_str());
          std::fflush(stdout);
        } else if (fija != cfg.porApp.end()) {
          if (dry) {
            std::printf("[serve] (dry) %s ya está ajustada a %s, no se pregunta\n", app.c_str(), fija->second.c_str());
            std::fflush(stdout);
            if (unaVez) break;
          } else {
            hijo = spawnViewer(exe, w.uuid, fija->second, cfg.red, cfg.cada, redActiva, cfg.pantalla, redOtra, cfg.rastreo, cfg.suavizado, &cfg,
                               false, app + ":" + fija->second, true);
            hijoDesde = now;
            hijoUuid = w.uuid;
            hijoApp = strLower(w.appId);
            visorEscondidas = 0;
          hijoApp = strLower(w.appId);
          visorEscondidas = 0;
            std::printf("[serve] lanzando 3D (%s) sin preguntar por %s\n", fija->second.c_str(), app.c_str());
            std::fflush(stdout);
            if (hijo > 0 && unaVez) break;
          }
        } else if (cfg.modo == "ask" && hayTarjeta) {
          /* el titulo es el de fabrica de la tarjeta («¿Qué tipo de vídeo estás viendo?»); debajo,
             en letra chica, la ventana por la que se pregunta */
          std::string titulo;
          std::string cuerpo = w.title.empty() ? w.appId : w.title;
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
          std::string id = "odysseyvk-" + w.uuid.substr(0, 8);
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
          hijo = spawnViewer(exe, w.uuid, cfg.modo, cfg.red, cfg.cada, redActiva, cfg.pantalla, redOtra, cfg.rastreo, cfg.suavizado, &cfg);
          hijoDesde = now;
          hijoUuid = w.uuid;
          hijoApp = strLower(w.appId);
          visorEscondidas = 0;
          std::printf("[serve] lanzando 3D (%s) por %s | %s\n", cfg.modo.c_str(), w.appId.c_str(), w.title.c_str());
          std::fflush(stdout);
          if (hijo > 0 && unaVez) break;
        }
      }
    }

    /* El vigilante cuenta lo suyo —que sesion tiene abierta, en que pantalla y con que red— en su
       propio fichero, para que la ventana de ajustes sepa si su boton del overlay esta puesto o
       quitado. El visor cuenta los suyos en `estado`; aqui va lo que solo sabe el hub. */
    if (now - hubEstadoDesde >= 1.0) {
      hubEstadoDesde = now;
      char hora[32] = {0};
      const std::time_t t = std::time(nullptr);
      std::tm tmLocal{};
      localtime_r(&t, &tmLocal);
      std::strftime(hora, sizeof hora, "%Y-%m-%d %H:%M:%S", &tmLocal);
      std::string txt = "# Lo que esta haciendo el vigilante (lo escribe el hub, no el visor).\n";
      txt += std::string("hora = ") + hora + "\n";
      txt += "servicio = corriendo\n";
      txt += std::string("sesion = ") + (hijo > 0 ? (forzado ? "escritorio" : "video") : "ninguna") + "\n";
      txt += "ventana = " + (hijoUuid.empty() ? std::string("-") : hijoUuid) + "\n";
      txt += "sesion_modo = " + (hijo > 0 && !g_modoVisor.empty() ? g_modoVisor : std::string("-")) + "\n";
      txt += "pantalla = " + (cfg.pantalla.empty() ? std::string("(la del panel)") : cfg.pantalla) + "\n";
      txt += "modo = " + cfg.modo + "\n";
      txt += "red = " + std::to_string(redElegida) + "\n";   /* 1 o 2, las mismas palabras que la pastilla y que el visor */
      txt += "modelo = " + (redActiva.empty() ? std::string("(la de la pastilla)") : redActiva) + "\n";
      ajuste::escribirEstadoHub(txt);
    }
  }

  if (!avisoId.empty()) nt.remove(avisoId);
  if (hayTarjeta) ov->hideAsk();
  if (hijo > 0) {
    ::kill(hijo, SIGTERM);
    int st = 0;
    ::waitpid(hijo, &st, 0);
  }
  /* y el video vuelve a su lugar en el apilamiento: lo tuvimos con KeepBelow para que un click no
     tapara la trama, y si se queda asi el usuario ve su escritorio con el video escondido. */
  if (!hijoUuid.empty()) {
    WindowInfo videoAntes{};
    std::string e6;
    if (wm.find(hijoUuid, videoAntes) && videoAntes.mapped &&
        (videoAntes.states & WindowInfo::KeepBelow) &&
        !wm.setState(hijoUuid, WindowInfo::KeepBelow, false, &e6))
      std::printf("[ventana] no se le pudo quitar el debajo al video %s: %s\n",
                  hijoUuid.c_str(), e6.c_str());
    std::fflush(stdout);
  }
  devolverDemas();              /* y las demas ventanas que bajamos, todas de vuelta a su lugar */
  /* Al parar el servicio el visor se va con nosotros y restaura el panel el solo; si no llego a
     hacerlo (lo mato systemd, o revento antes), el monitor quedaba en modo 3D hasta el reinicio. */
  /* Y quitamos nuestro chivato del disco: mientras el servicio vive escribimos en `hub` una vez por
     segundo, y si el fichero se quedara tras parar, la ventana de ajustes creeriamos que sigue habiendo
     un vigilante con alguna sesion abierta. */
  {
    std::error_code ec;
    std::filesystem::remove(ajuste::rutaHub(), ec);
  }
  devolverPanelHuerfano("el servicio se detuvo");
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
  /* Desde v0.57 el proyecto se llama OdysseyVK: si el usuario todavia tiene sus datos en la
     carpeta del nombre viejo, los copiamos antes de tocar nada (no borramos lo viejo). */
  migrarDatosDeViejoNombre();
  /* el idioma de la interfaz (v1.07): `idioma =` de serve.conf, ingles si no esta */
  idioma::cargar();
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
    if (sub == "vista") return cmdVistaCamara(argc - 3, argv + 3);
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
    if (sub == "planos") return cmdPlanos(argc - 3, argv + 3);
    if (sub == "ajuste") return cmdAjuste(argc - 3, argv + 3);
    if (sub == "atajos") return cmdAtajos(argc - 3, argv + 3);
    if (sub == "estado") return cmdEstado();
    if (sub == "modelos") return cmdModelos();
    if (sub == "preferencias") return cmdPreferencias(argc - 3, argv + 3);
    if (sub == "escritorio") return cmdEscritorio(argc - 3, argv + 3);
    if (sub == "capa") return cmdCapa(argc - 3, argv + 3);
    if (sub == "barra") {
      /* `ctl barra guardar [N]`: le pide al visor abierto que deje N copias chicas del cuadro (16 si
         no se dice) para afinar el detector de la barra del reproductor; `ctl barra` dice donde. */
      const std::string que = argc > 3 ? strLower(argv[3]) : std::string();
      if (que != "guardar") {
        std::printf("uso: odysseyvk ctl barra guardar [N]   (las deja en %s)\n", ajuste::rutaBarra().c_str());
        return que.empty() ? 0 : 2;
      }
      const int cuantas = argc > 4 ? std::max(1, std::min(60, std::atoi(argv[4]))) : 16;
      /* con su hora: el visor solo atiende la orden si tiene menos de cinco minutos */
      const std::string n = std::to_string(cuantas) + "@" + std::to_string((long long)std::time(nullptr));
      std::string err;
      if (!ajuste::escribir(std::map<std::string, double>(), std::string(), {{"barra", n}}, &err)) {
        std::fprintf(stderr, "%s\n", err.c_str());
        return 1;
      }
      std::printf("pedido: el visor guardara copias chicas del cuadro en %s mientras este en video dividido\n",
                  ajuste::rutaBarra().c_str());
      return 0;
    }
    if (sub == "red") {
      /* `ctl red`: que red de profundidad hay; `ctl red descargar`: baja la abierta a su carpeta, con
         el mismo camino que el boton del panel de ajustes (v0.92), y espera a que acabe. */
      const std::string que = argc > 3 ? strLower(argv[3]) : std::string();
      const bool hay = prefs::redAbiertaPresente();
      if (que != "descargar") {
        std::printf("red abierta (Depth Anything V2 pequena): %s\n  %s\n", hay ? "en su sitio" : "FALTA", rutaRedNuestra().c_str());
        std::printf("se baja de %s\n", prefs::kUrlRedAbierta);
        if (!hay) std::printf("  odysseyvk ctl red descargar   la deja ahi (95 MB)\n");
        return hay ? 0 : 1;
      }
      if (hay) {
        std::printf("ya esta: %s\n", rutaRedNuestra().c_str());
        return 0;
      }
      std::string err;
      if (!prefs::descargar(&err)) {
        std::fprintf(stderr, "%s\n", err.c_str());
        return 1;
      }
      std::printf("descargando %s\n", prefs::kUrlRedAbierta);
      while (prefs::descargaEnMarcha()) {
        if (prefs::vigilarDescarga()) {
          std::printf("  %s\n", prefs::textoDescarga().c_str());
          std::fflush(stdout);
        }
        ::usleep(500000);
      }
      return prefs::redAbiertaPresente() ? 0 : 1;
    }
    if (sub == "3d") return cmd3D(argc - 3, argv + 3);
    if (sub == "legado") return cmdLegado(argc - 3, argv + 3);
    if (sub == "mapa") return cmdMapa(argc - 3, argv + 3);
    std::fprintf(stderr, "subcomando desconocido: %s\n", sub.c_str());
    return 2;
  }
  if (cmd == "texto-detect")
    return cmdTexto(argc - 2, argv + 2);
  if (cmd == "barra-detect")
    return cmdBarra(argc - 2, argv + 2);
  if (cmd == "ajustes")
    return cmdAjustes(argc - 2, argv + 2);
  if (cmd == "planos")
    return cmdPlanos(argc - 2, argv + 2);
  if (cmd == "mapa")
    return cmdMapa(argc - 2, argv + 2);
  if (cmd == "actualizar") return cmdActualizar(argc - 2, argv + 2);
  if (cmd == "desinstalar") return cmdDesinstalar(argc - 2, argv + 2);
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
    std::printf("odysseyvk %s\n", ODYSSEYVK_VERSION);
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
        /* sin archivo (el lanzador fijado en la barra de tareas, v1.05): el panel de ajustes */
        std::fprintf(stderr, "sin archivo: se abre el panel de ajustes (para un video: odysseyvk play ARCHIVO)\n");
        char *args[] = {argv[0], (char *)"ajustes", nullptr};
        return cmdAjustes(0, args + 2);
      }
      return runViewer(o, SourceKind::Mpv);
    }
    return hasFlag(argc - 2, argv + 2, "--gl") ? runViewer(o, SourceKind::Test)
                                               : runViewerVk(o, SourceKind::Test);
  }
  std::fputs(kUsage, stderr);
  return 2;
}
