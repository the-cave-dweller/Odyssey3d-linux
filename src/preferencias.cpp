/* preferencias.cpp — ver preferencias.h. */
#include "preferencias.h"

#include "monitor.h"

#include <stdexcept>
#include "idioma.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <map>
#include <set>
#include <system_error>

#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "bususuario.h"

extern char **environ;

#include "ajuste.h"
#include "atajos.h"
#include "integraciones.h"
#include "mako.h"   /* (v1.18) MAKO FG: su boton de instalar, junto a ENABLE_MAKO */
#include "efectos.h"   /* (v1.18) los efectos de ReShade: General y el icono de cada caratula */
#include "util.h"

#if defined(HAVE_JPEG)
#include <jpeglib.h>
#elif defined(HAVE_OPENCV)
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#endif

namespace prefs {
namespace {

bool leeTexto(const std::string &ruta, std::string &sal) {
  FILE *f = std::fopen(ruta.c_str(), "re");
  if (!f) return false;
  char buf[8192];
  size_t n;
  sal.clear();
  while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) sal.append(buf, n);
  std::fclose(f);
  return true;
}

/* el valor de `"clave"   "valor"` en un fichero de Steam (VDF): la primera vez que sale la clave */
std::string valorVdf(const std::string &txt, const std::string &clave) {
  const std::string aguja = "\"" + clave + "\"";
  size_t p = txt.find(aguja);
  if (p == std::string::npos) return {};
  p = txt.find('"', p + aguja.size());
  if (p == std::string::npos) return {};
  std::string v;
  for (size_t i = p + 1; i < txt.size() && txt[i] != '"'; i++) {
    if (txt[i] == '\\' && i + 1 < txt.size()) i++;
    v += txt[i];
  }
  return v;
}

std::vector<std::string> todosVdf(const std::string &txt, const std::string &clave) {
  std::vector<std::string> sal;
  const std::string aguja = "\"" + clave + "\"";
  size_t p = 0;
  while ((p = txt.find(aguja, p)) != std::string::npos) {
    size_t q = txt.find('"', p + aguja.size());
    if (q == std::string::npos) break;
    std::string v;
    size_t i = q + 1;
    for (; i < txt.size() && txt[i] != '"'; i++) {
      if (txt[i] == '\\' && i + 1 < txt.size()) i++;
      v += txt[i];
    }
    sal.push_back(v);
    p = i;
  }
  return sal;
}

/* La caratula vertical de un juego en la cache de Steam: `library_600x900.jpg`, suelta o dentro de
   una carpeta con nombre de huella; si no, `library_capsule.jpg`, que en la cache nueva es lo mismo. */
void cargaCaratula(const std::string &raizSteam, Juego &j) {
#if defined(HAVE_JPEG) || defined(HAVE_OPENCV)
  const std::string dir = raizSteam + "/appcache/librarycache/" + j.id;
  std::error_code ec;
  std::string mejor, segunda;
  for (auto it = std::filesystem::recursive_directory_iterator(dir, ec); !ec && it != std::filesystem::recursive_directory_iterator();
       it.increment(ec)) {
    const std::string n = it->path().filename().string();
    if (n == "library_600x900.jpg") mejor = it->path().string();
    else if (n == "library_capsule.jpg") segunda = it->path().string();
  }
  const std::string ruta = !mejor.empty() ? mejor : segunda;
  if (ruta.empty()) return;
#if defined(HAVE_JPEG)
  /* (v1.16) con libjpeg, sin OpenCV: asi el panel ligero (odysseyvk-panel) no carga OpenCV. Se descomprime ya a la
     mitad o menos (la de Steam mide 600 x 900: sale 300 x 450) y se baja a 280 x 420 con la media de cada bloque,
     como el INTER_AREA de antes */
  FILE *f = std::fopen(ruta.c_str(), "rbe");
  if (!f) return;
  jpeg_decompress_struct cinfo{};
  jpeg_error_mgr jerr{};
  cinfo.err = jpeg_std_error(&jerr);
  jerr.error_exit = [](j_common_ptr c) { throw std::runtime_error(c->err->jpeg_message_table[c->err->msg_code]); };
  std::vector<uint8_t> src;
  int sw = 0, sh = 0;
  try {
    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, f);
    jpeg_read_header(&cinfo, TRUE);
    /* las de cuatro componentes (CMYK o YCCK, como la de Alice: Madness Returns) no las pasa libjpeg a RGB: se piden en
       CMYK y se convierten aqui (las de Adobe guardan la tinta al reves) */
    const bool cmyk = cinfo.jpeg_color_space == JCS_CMYK || cinfo.jpeg_color_space == JCS_YCCK;
    const bool adobe = cinfo.saw_Adobe_marker;
    cinfo.out_color_space = cmyk ? JCS_CMYK : JCS_RGB;
    cinfo.scale_num = 1;
    cinfo.scale_denom = 1;
    while (cinfo.scale_denom < 8 && (int)cinfo.image_width / (int)(cinfo.scale_denom * 2) >= 280 &&
           (int)cinfo.image_height / (int)(cinfo.scale_denom * 2) >= 420)
      cinfo.scale_denom *= 2;
    jpeg_start_decompress(&cinfo);
    sw = (int)cinfo.output_width, sh = (int)cinfo.output_height;
    src.resize((size_t)sw * sh * 3u);
    std::vector<uint8_t> fila4(cmyk ? (size_t)sw * 4u : 0u);
    while (cinfo.output_scanline < cinfo.output_height) {
      const size_t y = cinfo.output_scanline;
      JSAMPROW fila = cmyk ? fila4.data() : src.data() + y * sw * 3u;
      jpeg_read_scanlines(&cinfo, &fila, 1);
      if (cmyk)
        for (int x = 0; x < sw; x++) {
          const uint8_t *q = fila4.data() + (size_t)x * 4u;
          for (int c = 0; c < 3; c++) {
            const unsigned t = adobe ? q[c] : 255u - q[c], k = adobe ? q[3] : 255u - q[3];
            src[(y * sw + x) * 3u + c] = (uint8_t)((t * k + 127u) / 255u);
          }
        }
    }
    jpeg_finish_decompress(&cinfo);
  } catch (const std::exception &) {
    sw = sh = 0;
  }
  jpeg_destroy_decompress(&cinfo);
  std::fclose(f);
  if (sw < 16 || sh < 16) return;
  const int dw = 280, dh = 420;
  auto px = std::make_shared<std::vector<uint8_t>>((size_t)dw * dh * 4u);
  for (int y = 0; y < dh; y++) {
    const int y0 = y * sh / dh, y1 = std::max(y0 + 1, (y + 1) * sh / dh);
    for (int x = 0; x < dw; x++) {
      const int x0 = x * sw / dw, x1 = std::max(x0 + 1, (x + 1) * sw / dw);
      unsigned suma[3] = {0, 0, 0}, n = 0;
      for (int yy = y0; yy < y1; yy++)
        for (int xx = x0; xx < x1; xx++, n++)
          for (int c = 0; c < 3; c++) suma[c] += src[((size_t)yy * sw + xx) * 3u + c];
      uint8_t *d = px->data() + ((size_t)y * dw + x) * 4u;
      for (int c = 0; c < 3; c++) d[c] = (uint8_t)((suma[c] + n / 2) / n);
      d[3] = 255;
    }
  }
  j.cara = px;
  j.caraW = dw, j.caraH = dh;
#else
  cv::Mat m = cv::imread(ruta, cv::IMREAD_COLOR);
  if (m.empty() || m.cols < 16 || m.rows < 16) return;
  cv::Mat chico, rgba;
  cv::resize(m, chico, cv::Size(280, 420), 0, 0, cv::INTER_AREA);
  cv::cvtColor(chico, rgba, cv::COLOR_BGR2RGBA);
  auto px = std::make_shared<std::vector<uint8_t>>((size_t)rgba.cols * rgba.rows * 4u);
  for (int y = 0; y < rgba.rows; y++) std::memcpy(px->data() + (size_t)y * rgba.cols * 4u, rgba.ptr(y), (size_t)rgba.cols * 4u);
  j.cara = px;
  j.caraW = rgba.cols, j.caraH = rgba.rows;
#endif
#else
  (void)raizSteam;
  (void)j;
#endif
}

/* (v1.16, de la PR #2 de Noa3) si un juego entra en 3D: lo que tenga apuntado (el ultimo, como lo lee el servicio) o,
   sin nada, lo de `modo` (con `modo = flat` se queda en 2D). El interruptor de Juegos y el lanzarlo dicen lo mismo */
bool juegoEn3D(const Valores &v, const std::string &id) {
  const std::string app = "steam_app_" + id;
  for (auto it = v.recordadas.rbegin(); it != v.recordadas.rend(); ++it)
    if (it->first == app) return it->second != "flat";
  return v.modo != "flat";
}

bool esHerramienta(const std::string &nombre) {
  static const char *kPrefijos[] = {"Proton ", "Steam Linux Runtime", "Steamworks Common", "Steam Controller"};
  for (const char *p : kPrefijos)
    if (nombre.rfind(p, 0) == 0) return true;
  return nombre == "Proton" || nombre.empty();
}

bool si(const std::string &v, bool porDefecto) {
  const std::string s = strLower(v);
  if (s.empty()) return porDefecto;
  return !(s == "no" || s == "0" || s == "off" || s == "apagado" || s == "quitado" || s == "false");
}

double numero(const std::string &v, double porDefecto) {
  if (v.empty()) return porDefecto;
  std::string s = v;
  for (char &c : s)
    if (c == ',') c = '.';
  char *fin = nullptr;
  const double d = std::strtod(s.c_str(), &fin);
  return fin == s.c_str() ? porDefecto : d;
}

std::string conComa(double v, int decimales) {
  char buf[48];
  std::snprintf(buf, sizeof buf, "%.*f", decimales, v);
  std::string s = buf;
  for (char &c : s)
    if (c == '.') c = ',';
  return s;
}

namespace {
/* para ensenar (no para serve.conf, que va siempre con coma): con la coma o el punto del idioma */
std::string conDecimal(double v, int decimales) {
  std::string s = conComa(v, decimales);
  for (char &c : s)
    if (c == ',') c = idioma::decimal();
  return s;
}
/* (v1.16, de la PR #1 de Noa3) cada deslizador: de su fraccion al valor que se guarda, redondeado igual al soltar que al
   ensenarlo mientras se arrastra (el numero de la derecha sigue a la bola y dice lo que se va a guardar) */
float fr01(float f) { return std::max(0.0f, std::min(1.0f, f)); }
double relieveDe(float f) { return std::round((0.5 + fr01(f) * 1.5) / 0.05) * 0.05; }
double fantasmaDe(float f) { return std::round(fr01(f) * 0.10 / 0.002) * 0.002; }
double mezclaDe(float f) { return std::round((0.10 + fr01(f) * 0.90) / 0.05) * 0.05; }
int topeCapaDe(float f) {
  int n = (int)std::lround(fr01(f) * 48.0f) * 5;
  return n > 0 && n < 10 ? 10 : n;
}
int anticipacionDe(float f) { return (int)std::lround(fr01(f) * 60.0f); }
std::string textoRelieve(float f) { return conDecimal(relieveDe(f), 2); }
std::string textoFantasma(float f) { return conDecimal(fantasmaDe(f) * 100.0, 1) + " %"; }
std::string textoMezcla(float f) { return std::to_string((int)std::lround(mezclaDe(f) * 100.0)) + " %"; }
std::string textoTopeCapa(float f) {
  const int n = topeCapaDe(f);
  return n > 0 ? std::to_string(n) + " fps" : std::string(idioma::tr("sin tope"));
}
std::string textoAnticipacion(float f) { return std::to_string(anticipacionDe(f)) + " ms"; }
}  // namespace

const char *nombreModo(const std::string &m) {
  if (m == "conv") return idioma::tr("vídeo normal");
  if (m == "sbs" || m == "suave") return idioma::tr("vídeo dividido");
  if (m == "tb") return idioma::tr("arriba y abajo");
  if (m == "flat") return idioma::tr("sin 3D");
  return m.c_str();
}

/* avisar al visor abierto (si lo hay): una orden suelta por el canal de ajuste */
void avisarVisor(const std::map<std::string, double> &numeros, const std::map<std::string, std::string> &palabras) {
  std::string e;
  ajuste::escribir(numeros, std::string(), palabras, &e);
}

/* (v1.14) lo que hay que poner en las opciones de lanzamiento de un juego de Steam para la profundidad de DLSS */
const char *const kParametroSteam = "ODYSSEYVK=1 %command%";

/* (v1.14) un texto al portapapeles del escritorio. En Wayland el portapapeles de una ventana se va con ella (y el
   panel se cierra al picar fuera), asi que en KDE se le da a Klipper, el portapapeles de Plasma, por el bus de
   sesion (org.kde.klipper /klipper setClipboardContents): se lo queda el. Sin Klipper (otro escritorio), wl-copy si
   esta instalado (se queda en segundo plano sirviendolo). */
bool copiarAlPortapapeles(const std::string &texto, std::string *err) {
#ifdef HAVE_SDBUS
  sd_bus *bus = nullptr;
  if (abrirBusUsuario(&bus) >= 0) {
    sd_bus_error e = SD_BUS_ERROR_NULL;
    const int r = sd_bus_call_method(bus, "org.kde.klipper", "/klipper", "org.kde.klipper.klipper", "setClipboardContents",
                                     &e, nullptr, "s", texto.c_str());
    sd_bus_error_free(&e);
    sd_bus_flush_close_unref(bus);
    if (r >= 0) return true;
  }
#endif
  pid_t pid = 0;
  const char *argv[] = {"wl-copy", "--", texto.c_str(), nullptr};
  if (posix_spawnp(&pid, "wl-copy", nullptr, nullptr, (char *const *)argv, environ) == 0) {
    int st = 0;
    if (waitpid(pid, &st, 0) == pid && WIFEXITED(st) && WEXITSTATUS(st) == 0) return true;
  }
  if (err) *err = idioma::tr("no hay portapapeles de KDE ni wl-copy");
  return false;
}

/* (v1.15) lo que hay en el portapapeles (Ctrl+V en un campo de texto): de Klipper o, sin el, de wl-paste */
bool leerPortapapeles(std::string &texto) {
  texto.clear();
#ifdef HAVE_SDBUS
  sd_bus *bus = nullptr;
  if (abrirBusUsuario(&bus) >= 0) {
    sd_bus_error e = SD_BUS_ERROR_NULL;
    sd_bus_message *m = nullptr;
    const int r = sd_bus_call_method(bus, "org.kde.klipper", "/klipper", "org.kde.klipper.klipper", "getClipboardContents",
                                     &e, &m, "");
    const char *t = nullptr;
    const bool ok = r >= 0 && sd_bus_message_read(m, "s", &t) >= 0 && t;
    if (ok) texto = t;
    sd_bus_message_unref(m);
    sd_bus_error_free(&e);
    sd_bus_flush_close_unref(bus);
    if (ok) return true;
  }
#endif
  FILE *p = ::popen("wl-paste -n 2>/dev/null", "r");
  if (!p) return false;
  char buf[4096];
  size_t n;
  while ((n = std::fread(buf, 1, sizeof buf, p)) > 0 && texto.size() < 65536) texto.append(buf, n);
  return ::pclose(p) == 0;
}

}  // namespace

std::vector<Juego> juegosSteam(bool conCaratulas) {
  std::vector<Juego> sal;
  const char *home = std::getenv("HOME");
  if (!home) return sal;
  std::vector<std::string> raices;
  for (const char *rel : {"/.local/share/Steam", "/.steam/steam", "/.var/app/com.valvesoftware.Steam/.local/share/Steam"}) {
    const std::string base = std::string(home) + rel;
    std::string vdf;
    if (!leeTexto(base + "/steamapps/libraryfolders.vdf", vdf)) continue;
    raices.push_back(base);
    for (const std::string &p : todosVdf(vdf, "path")) raices.push_back(p);
  }
  std::vector<std::string> vistas, ids;
  for (const std::string &r : raices) {
    std::error_code ec;
    const std::filesystem::path real = std::filesystem::weakly_canonical(r, ec);
    const std::string clave = ec ? r : real.string();
    if (std::find(vistas.begin(), vistas.end(), clave) != vistas.end()) continue;
    vistas.push_back(clave);
    for (const auto &e : std::filesystem::directory_iterator(clave + "/steamapps", ec)) {
      const std::string n = e.path().filename().string();
      if (n.rfind("appmanifest_", 0) != 0 || n.size() < 17 || n.substr(n.size() - 4) != ".acf") continue;
      std::string txt;
      if (!leeTexto(e.path().string(), txt)) continue;
      Juego j;
      j.id = valorVdf(txt, "appid");
      j.nombre = valorVdf(txt, "name");
      j.ultimaVez = std::atoll(valorVdf(txt, "LastPlayed").c_str());
      /* StateFlags 4 = instalado del todo; a medio bajar no se ofrece */
      if (j.id.empty() || esHerramienta(j.nombre) || !(std::atoi(valorVdf(txt, "StateFlags").c_str()) & 4)) continue;
      if (std::find(ids.begin(), ids.end(), j.id) != ids.end()) continue;
      ids.push_back(j.id);
      if (conCaratulas) cargaCaratula(raices.front(), j);
      sal.push_back(j);
    }
  }
  std::sort(sal.begin(), sal.end(), [](const Juego &a, const Juego &b) {
    if (a.ultimaVez != b.ultimaVez) return a.ultimaVez > b.ultimaVez;
    return a.nombre < b.nombre;
  });
  return sal;
}

bool lanzarJuego(const std::string &id, std::string *err) {
  for (char c : id)
    if (c < '0' || c > '9') {
      if (err) *err = idioma::tr("ese no es un juego de Steam");
      return false;
    }
  const std::string url = "steam://rungameid/" + id;
  const pid_t pid = ::fork();
  if (pid < 0) {
    if (err) *err = std::string("no se pudo arrancar: ") + std::strerror(errno);
    return false;
  }
  if (pid == 0) {
    /* suelto del todo: sesion propia y sin nuestros descriptores, para que siga vivo al cerrar el panel */
    ::setsid();
    if (::fork() != 0) ::_exit(0);
    const int nulo = ::open("/dev/null", O_RDWR);
    if (nulo >= 0) {
      ::dup2(nulo, 0);
      ::dup2(nulo, 1);
      ::dup2(nulo, 2);
    }
    ::execlp("steam", "steam", url.c_str(), (char *)nullptr);
    ::execlp("xdg-open", "xdg-open", url.c_str(), (char *)nullptr);
    ::_exit(127);
  }
  int st = 0;
  ::waitpid(pid, &st, 0);
  return true;
}

Valores leer() {
  Valores v;
  const std::map<std::string, std::string> c = ajuste::leerServeConf();
  auto de = [&](const char *k) {
    const auto it = c.find(k);
    return it == c.end() ? std::string() : it->second;
  };
  const std::string modo = strLower(de("modo"));
  /* (v1.16, PR #2) `flat` se respeta: lo que no este apuntado se queda en 2D (antes se leia como `ask`) */
  v.modo = (modo == "conv" || modo == "sbs" || modo == "flat") ? modo : (modo == "suave" ? "sbs" : "ask");
  {
    const std::string ds = strLower(de("dividido_suavizado"));
    v.suavizar = (ds == "auto" || ds == "automatico") ? 2 : (si(ds, true) ? 1 : 0);
  }
  v.rastreo = si(de("rastreo"), false);
  v.atajos = si(de("atajos_globales"), true);
  v.fantasma = std::max(0.0, std::min(0.10, numero(de("crosstalk_base"), 0.0128535)));
  v.mezclaVistas = std::max(0.10, std::min(1.0, 1.0 / std::max(1.0, numero(de("filtro"), 10.0))));
  v.relieve = std::max(0.5, std::min(2.0, numero(de("relieve"), 1.0)));
  v.modelo = de("modelo");
  v.nuestra = de("nuestra");
  v.modeloVideo = de("modelo_video");
  v.avisarActualizaciones = si(de("actualizaciones"), true);
  v.actualizarAuto = si(de("actualizar_auto"), false);
  v.enBarraTareas = si(de("barra_tareas"), true);
  v.enBandeja = si(de("bandeja"), true);
  v.expRed = de("exp_red");
  if (v.expRed != "518" && v.expRed != "770") v.expRed = "cuadrado";   /* 1022 no abre en CUDA */
  v.expMapa = std::atoi(de("exp_mapa").c_str());
  if (v.expMapa != 770 && v.expMapa != 1022) v.expMapa = 518;
  v.expTope = si(de("exp_tope_arrastre"), false);
  v.expAnticipacion = std::max(0, std::min(60, std::atoi(de("exp_anticipacion").c_str())));
  {
    std::string t = de("exp_topes_red");
    for (char &c : t)
      if (c == ',') c = '.';
    v.expTopesRed = t.empty() ? 1.0 : std::max(1.0, std::min(4.0, std::atof(t.c_str())));
  }
  v.expSinRed = de("exp_sin_red");
  {
    const std::string antes = v.expSinRed;
    v.expSinRed = (antes == "si" || antes == "suelo" || antes == "imagen") ? "si" : "no";
  }
  v.vrrEn3d = si(de("vrr_en_3d"), false);
  v.profDlss = si(de("profundidad_dlss"), true);
  v.hudPlano = si(de("hud_plano"), true);
  v.fpsJuego = si(de("fps_juego"), true);
  v.capaTope = std::max(0, std::min(240, std::atoi(de("capa_fps_tope").c_str())));
  v.expPrioridad = de("exp_prioridad");
  if (v.expPrioridad != "alta" && v.expPrioridad != "tiempo_real") v.expPrioridad = "normal";
  for (std::string *m : {&v.modelo, &v.nuestra, &v.modeloVideo})
    if (!m->empty() && (*m)[0] == '~') {
      const char *home = std::getenv("HOME");
      *m = std::string(home ? home : "") + m->substr(1);
    }
  /* las lineas `auto = app:modo, otra:modo` pueden ser varias: se lee el fichero entero */
  std::string txt;
  if (leeTexto(ajuste::rutaServeConf(), txt)) {
    size_t ini = 0;
    while (ini < txt.size()) {
      size_t fin = txt.find('\n', ini);
      if (fin == std::string::npos) fin = txt.size();
      std::string l = strTrim(txt.substr(ini, fin - ini));
      ini = fin + 1;
      const size_t eq = l.find('=');
      if (l.empty() || l[0] == '#' || eq == std::string::npos || strLower(strTrim(l.substr(0, eq))) != "auto") continue;
      std::string par;
      for (char ch : l.substr(eq + 1) + ",") {
        if (ch != ',' && ch != ' ' && ch != '\t') {
          par += ch;
          continue;
        }
        const size_t dd = par.find(':');
        if (dd != std::string::npos && dd > 0) {
          const std::string app = strLower(par.substr(0, dd)), md = strLower(par.substr(dd + 1));
          bool estaba = false;
          for (auto &r : v.recordadas)
            if (r.first == app) r.second = md, estaba = true;
          if (!estaba) v.recordadas.emplace_back(app, md);
        }
        par.clear();
      }
    }
  }
  return v;
}

/* ---- la red abierta y su descarga (v0.92) ------------------------------------------------------ */
/* desde la rama experimental se baja la fp16 (49,6 MB); la de siempre (fp32, 99 MB) sigue valiendo y desde la 1.16
   tambien se ofrece (Rodolfo: «ofrece descargar la red FP32 tambien, sugiere por defecto la fp16») */
const char *const kUrlRedAbierta = "https://huggingface.co/onnx-community/depth-anything-v2-small/resolve/main/onnx/model_fp16.onnx";
const char *const kUrlRedFp32 = "https://huggingface.co/onnx-community/depth-anything-v2-small/resolve/main/onnx/model.onnx";
/* (v1.16) la red de video: Video Depth Anything pequena (Apache-2.0), exportada a ONNX por la comunidad (looplj); no hay
   una oficial. Fijada a la revision que se midio el 27-nov (su SHA-256, eb5926ad…bb99) */
const char *const kUrlRedVideo = "https://huggingface.co/looplj/video-depth-anything-small-onnx/resolve/"
                                 "ff810849333df46d37ea18a885fc11689661fe96/video-depth-anything-small-dynamic-fp32.onnx";

namespace {
bool tamanoEs(const std::string &ruta, long bytes) {
  struct stat sb {};
  return ::stat(ruta.c_str(), &sb) == 0 && sb.st_size == bytes;
}
}  // namespace

bool redLigeraPresente() { return tamanoEs(rutaRedLigera(), kBytesRedAbierta); }
bool redFp32Presente() { return tamanoEs(rutaRedVieja(), kBytesRedVieja); }
bool redVideoPresente() { return tamanoEs(rutaRedVideo(), kBytesRedVideo); }
bool redAbiertaPresente() { return redLigeraPresente() || redFp32Presente(); }

namespace {
struct Descarga {
  pid_t pid = 0;            /* el curl, mientras corre */
  int cual = 0;             /* 0 = la fp16, 1 = la fp32 */
  std::string parte;        /* el archivo a medias */
  std::string texto;        /* lo que ensena el panel */
  std::string lista;        /* (v1.16) la que acaba de bajar entera (para ponerla), hasta que se recoge */
  bool fallo = false;
  bool habiaRed = true;     /* (v1.16) al empezar ya habia alguna red (si no, el panel se cierra al acabar) */
  long bytes = 0;
} g_descarga;
const char *urlDe(int cual) {
  if (const char *u = entornoOdyssey("ODYSSEYVK_RED_URL")) return u;   /* para las pruebas: un file:// en vez de Hugging Face */
  return cual == 2 ? kUrlRedVideo : (cual == 1 ? kUrlRedFp32 : kUrlRedAbierta);
}
std::string destinoDe(int cual) { return cual == 2 ? rutaRedVideo() : (cual == 1 ? rutaRedVieja() : rutaRedLigera()); }
long bytesDe(int cual) { return cual == 2 ? kBytesRedVideo : (cual == 1 ? kBytesRedVieja : kBytesRedAbierta); }
std::string megasDe(int cual) { return cual == 2 ? "117" : (cual == 1 ? "99" : "50"); }
}  // namespace

bool descargaEnMarcha() { return g_descarga.pid > 0; }
int descargaCual() { return g_descarga.pid > 0 ? g_descarga.cual : -1; }
std::string textoDescarga() { return g_descarga.texto; }
bool descargaHabiaRed() { return g_descarga.habiaRed; }
std::string recogerRedBajada() {
  std::string r;
  r.swap(g_descarga.lista);
  return r;
}

bool descargar(std::string *err) { return descargar(0, err); }

bool descargar(int cual, std::string *err) {
  if (g_descarga.pid > 0) return true;
  cual = cual == 1 || cual == 2 ? cual : 0;   /* (v1.16) 2 = la de video */
  const std::string destino = destinoDe(cual);
  const std::string dir = destino.substr(0, destino.rfind('/'));
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  g_descarga.habiaRed = !redPuesta(leer()).empty();
  g_descarga.cual = cual;
  g_descarga.lista.clear();
  g_descarga.parte = destino + ".parte";
  ::unlink(g_descarga.parte.c_str());
  const pid_t pid = ::fork();
  if (pid < 0) {
    if (err) *err = std::string("no se pudo arrancar la descarga: ") + std::strerror(errno);
    return false;
  }
  if (pid == 0) {
    /* el hijo: curl a su aire, sin heredar la terminal ni nada nuestro */
    const int nulo = ::open("/dev/null", O_RDWR);
    if (nulo >= 0) { ::dup2(nulo, 0); ::dup2(nulo, 1); ::dup2(nulo, 2); }
    ::execlp("curl", "curl", "-fsSL", "--max-time", "1800", "--retry", "2", "-o", g_descarga.parte.c_str(),
             urlDe(cual), (char *)nullptr);
    ::_exit(127);
  }
  g_descarga.pid = pid;
  g_descarga.fallo = false;
  g_descarga.bytes = 0;
  g_descarga.texto = idioma::trf("descargando… {} de {} MB", {"0", megasDe(cual)});
  return true;
}

bool vigilarDescarga() {
  if (g_descarga.pid <= 0) return false;
  int estado = 0;
  const pid_t r = ::waitpid(g_descarga.pid, &estado, WNOHANG);
  const int cual = g_descarga.cual;
  if (r == 0) {
    struct stat sb {};
    const long bytes = ::stat(g_descarga.parte.c_str(), &sb) == 0 ? (long)sb.st_size : 0;
    if (bytes / 1000000 == g_descarga.bytes / 1000000 && bytes != 0) return false;
    g_descarga.bytes = bytes;
    g_descarga.texto = idioma::trf("descargando… {} de {} MB", {std::to_string(bytes / 1000000), megasDe(cual)});
    return true;
  }
  g_descarga.pid = 0;
  const bool bien = r > 0 && WIFEXITED(estado) && WEXITSTATUS(estado) == 0;
  const bool entera = tamanoEs(g_descarga.parte, bytesDe(cual));
  if (bien && entera && ::rename(g_descarga.parte.c_str(), destinoDe(cual).c_str()) == 0) {
    g_descarga.texto = idioma::tr(cual == 2 ? "lista: la red de vídeo ya está en su carpeta"
                                            : (cual == 1 ? "lista: la FP32 ya está en su carpeta" : "lista: la red abierta ya está en su carpeta"));
    g_descarga.fallo = false;
    g_descarga.lista = destinoDe(cual);
  } else {
    ::unlink(g_descarga.parte.c_str());
    g_descarga.fallo = true;
    g_descarga.texto = !bien ? (r > 0 && WIFEXITED(estado) && WEXITSTATUS(estado) == 127
                                    ? idioma::tr("falta `curl`: instálalo y vuelve a intentarlo")
                                    : idioma::tr("la descarga falló (¿hay conexión?): vuelve a intentarlo"))
                             : idioma::tr("el archivo bajó incompleto: vuelve a intentarlo");
  }
  return true;
}

/* ---- (v1.16) las redes de la carpeta y la que va puesta ----------------------------------------- */
std::string redPuesta(const Valores &v) {
  /* lo mismo que elige el servicio (elegirRedes, src/main.cpp): `modelo =` si esta; si no, la del programa de Samsung
     si esta en la carpeta; si no, la abierta (la fp16 antes que la fp32) */
  if (!v.modelo.empty() && fileExists(v.modelo)) return std::filesystem::path(v.modelo).lexically_normal().string();
  const std::string samsung = carpetaRedes() + "mde_large.onnx";
  if (fileExists(samsung)) return samsung;
  if (!v.nuestra.empty() && fileExists(v.nuestra)) return std::filesystem::path(v.nuestra).lexically_normal().string();
  const std::string n = rutaRedNuestra();
  return fileExists(n) ? n : std::string();
}

std::vector<RedDisco> redesDelDisco(const Valores &v) {
  std::vector<RedDisco> r;
  r.push_back({rutaRedLigera(), "Depth Anything V2 pequeña · FP16", 1, redLigeraPresente(), kBytesRedAbierta});
  r.push_back({rutaRedVieja(), "Depth Anything V2 pequeña · FP32", 2, redFp32Presente(), kBytesRedVieja});
  /* cualquier otra .onnx de la carpeta (Rodolfo: «pueden colocar cualquier red en la carpeta ... y la herramienta la
     empezara a listar»), salvo el detector de caras del seguimiento de la cabeza */
  std::vector<RedDisco> otras;
  std::error_code ec;
  const std::string dir = carpetaRedes();
  for (const auto &e : std::filesystem::directory_iterator(dir, ec)) {
    std::error_code e2;
    const std::string nombre = e.path().filename().string();
    if (e.path().extension() != ".onnx" || !e.is_regular_file(e2)) continue;
    if (e.path().string() == rutaRedLigera() || e.path().string() == rutaRedVieja()) continue;
    if (nombre.rfind("face_detection", 0) == 0) continue;
    if (esRedDeVideo(nombre)) continue;   /* (v1.16) las de video van en su lista (redesDeVideo) */
    otras.push_back({e.path().string(), nombre.substr(0, nombre.size() - 5), 0, true, (long)e.file_size(e2)});
  }
  /* y la de `modelo =` si esta fuera de la carpeta */
  if (!v.modelo.empty() && fileExists(v.modelo) && !esRedDeVideo(v.modelo)) {
    const std::string m = std::filesystem::path(v.modelo).lexically_normal().string();
    bool ya = m == rutaRedLigera() || m == rutaRedVieja();
    for (const RedDisco &o : otras) ya = ya || o.ruta == m;
    if (!ya) {
      struct stat sb {};
      const std::string nombre = std::filesystem::path(m).filename().string();
      otras.push_back({m, nombre.size() > 5 ? nombre.substr(0, nombre.size() - 5) : nombre, 0, true,
                       ::stat(m.c_str(), &sb) == 0 ? (long)sb.st_size : 0});
    }
  }
  std::sort(otras.begin(), otras.end(), [](const RedDisco &a, const RedDisco &b) { return a.nombre < b.nombre; });
  r.insert(r.end(), otras.begin(), otras.end());
  return r;
}

bool ponerRed(const std::string &ruta, std::string *aviso) {
  std::string e;
  if (!ajuste::fijarEnServeConf("modelo", ruta, &e)) {
    if (aviso) *aviso = idioma::trf("no se pudo guardar: {}", {e});
    return false;
  }
  /* el visor que este abierto la cambia ya (lee el ajuste vivo cada decima); si no hay, la toma el siguiente */
  ajuste::escribir(std::map<std::string, double>(), ruta, &e);
  std::string nombre = std::filesystem::path(ruta).filename().string();
  for (const RedDisco &r : redesDelDisco(leer()))
    if (r.ruta == ruta) nombre = r.conocida ? std::string(idioma::tr(r.conocida == 1 ? "la FP16" : "la FP32")) : r.nombre;
  if (aviso) *aviso = idioma::trf("red puesta: {} (la cambia el visor abierto, o el siguiente vídeo)", {nombre});
  return true;
}

/* (v1.16) las redes de video: la que se baja desde aqui y cualquier .onnx de la carpeta con «video» en el nombre (y la
   de `modelo_video =` si esta fuera) */
std::vector<RedDisco> redesDeVideo(const Valores &v) {
  std::vector<RedDisco> r;
  r.push_back({rutaRedVideo(), "Video Depth Anything pequeña", 3, redVideoPresente(), kBytesRedVideo});
  std::error_code ec;
  for (const auto &e : std::filesystem::directory_iterator(carpetaRedes(), ec)) {
    std::error_code e2;
    const std::string nombre = e.path().filename().string();
    if (e.path().extension() != ".onnx" || !e.is_regular_file(e2) || !esRedDeVideo(nombre) || e.path().string() == rutaRedVideo()) continue;
    r.push_back({e.path().string(), nombre.substr(0, nombre.size() - 5), 0, true, (long)e.file_size(e2)});
  }
  if (!v.modeloVideo.empty() && fileExists(v.modeloVideo)) {
    const std::string m = std::filesystem::path(v.modeloVideo).lexically_normal().string();
    bool ya = false;
    for (const RedDisco &o : r) ya = ya || o.ruta == m;
    if (!ya) {
      struct stat sb {};
      const std::string nombre = std::filesystem::path(m).filename().string();
      r.push_back({m, nombre.size() > 5 ? nombre.substr(0, nombre.size() - 5) : nombre, 0, true,
                   ::stat(m.c_str(), &sb) == 0 ? (long)sb.st_size : 0});
    }
  }
  return r;
}

bool ponerRedVideo(const std::string &ruta, std::string *aviso) {
  std::string e;
  if (!ajuste::fijarEnServeConf("modelo_video", ruta, &e)) {
    if (aviso) *aviso = idioma::trf("no se pudo guardar: {}", {e});
    return false;
  }
  /* el visor abierto con un video se cambia ya; con un juego no hace caso (ver vkview.cpp) */
  ajuste::escribir(std::map<std::string, double>(), std::string(), {{"modelo_video", ruta.empty() ? std::string("no") : ruta}}, &e);
  if (aviso)
    *aviso = ruta.empty() ? std::string(idioma::tr("red de vídeo quitada: los vídeos vuelven a la de siempre"))
                          : std::string(idioma::tr("red de vídeo puesta: vale con los vídeos (la cambia el visor abierto, o el siguiente vídeo)"));
  return true;
}

/* ---- (v1.17) lo que vigila el cuadrito de los FPS ---------------------------------------------------- */
namespace {
std::set<std::string> monitorElegido() {
  const auto conf = ajuste::leerServeConf();
  const auto it = conf.find("monitor");
  return it == conf.end() ? std::set<std::string>() : monitor::deTexto(it->second);
}
}  // namespace

void opcionesMonitor(std::vector<std::string> &rotulos, std::vector<bool> &marcadas, std::vector<std::string> &claves) {
  rotulos.clear(), marcadas.clear(), claves.clear();
  const std::set<std::string> el = monitorElegido();
  auto pon = [&](const std::string &rot, const std::string &clave) {
    rotulos.push_back(rot), claves.push_back(clave), marcadas.push_back(el.count(clave) > 0);
  };
  for (const monitor::Tarjeta &t : monitor::leer().tarjetas) pon(idioma::trf("Tarjeta {}", {t.nombre}), t.clave);
  pon(idioma::tr("Temperatura de las tarjetas"), monitor::kTemp);
  pon(idioma::tr("Memoria de las tarjetas"), monitor::kVram);
  pon(idioma::tr("Consumo de las tarjetas"), monitor::kVatios);
  pon(idioma::tr("Uso del procesador"), monitor::kCpu);
  pon(idioma::tr("Temperatura del procesador"), monitor::kCpuTemp);
  pon(idioma::tr("Memoria del sistema"), monitor::kRam);
  pon(idioma::tr("Tiempo por cuadro del juego"), monitor::kMs);
}

bool alternarMonitor(const std::string &clave, std::string *aviso) {
  std::set<std::string> el = monitorElegido();
  const bool quitar = el.erase(clave) > 0;
  if (!quitar) el.insert(clave);
  const std::string texto = monitor::aTexto(el);
  std::string e;
  if (!ajuste::fijarEnServeConf("monitor", texto, &e)) {
    if (aviso) *aviso = idioma::trf("no se pudo guardar: {}", {e});
    return false;
  }
  ajuste::escribir(std::map<std::string, double>(), std::string(), {{"monitor", texto.empty() ? std::string("no") : texto}}, &e);
  if (aviso)
    *aviso = el.empty() ? std::string(idioma::tr("el cuadrito ya no vigila nada más"))
                        : idioma::trf("el cuadrito vigila {} cosas", {std::to_string(el.size())});
  return true;
}

/* ---- la actualizacion (v0.95) ---------------------------------------------------------------- */
const char *const kRepoGitHub = "https://github.com/the-cave-dweller/Odyssey3d-linux";
const char *const kUrlVersionRemota = "https://raw.githubusercontent.com/the-cave-dweller/Odyssey3d-linux/main/VERSION";
/* La API de GitHub da el archivo al momento. raw.githubusercontent pasa por una cache de hasta 5 min
   (MEDIDO el 26-nov 00:31: con la 1.00 ya subida, raw seguia dando 0.98, «x-cache: HIT») y una mirada
   que caiga en esa ventana deja la version vieja apuntada hasta la siguiente (v1.01). Se pide la API
   y, si falla (sin red o pasado su limite de 60 por hora), raw. */
const char *const kUrlVersionApi = "https://api.github.com/repos/the-cave-dweller/Odyssey3d-linux/contents/VERSION?ref=main";

bool versionMasNueva(const std::string &a, const std::string &b) {
  size_t i = 0, j = 0;
  while (i < a.size() || j < b.size()) {
    long na = 0, nb = 0;
    while (i < a.size() && a[i] >= '0' && a[i] <= '9') na = na * 10 + (a[i++] - '0');
    while (j < b.size() && b[j] >= '0' && b[j] <= '9') nb = nb * 10 + (b[j++] - '0');
    if (na != nb) return na > nb;
    if (i < a.size()) i++;
    if (j < b.size()) j++;
  }
  return false;
}

namespace {
std::string g_capturando;   /* la accion que espera su atajo nuevo (v1.09) */
/* las secciones plegables de Ajustes que estan desplegadas (rama experimental): de fabrica plegadas.
   Se recuerdan entre una apertura del panel y otra en ~/.local/state/odysseyvk/panel-secciones. */
std::set<std::string> &seccionesAbiertas() {
  static std::set<std::string> abiertas = [] {
    std::set<std::string> s;
    if (FILE *f = std::fopen(rutaEstadoOdyssey("panel-secciones").c_str(), "re")) {
      char linea[128];
      while (std::fgets(linea, sizeof linea, f)) {
        std::string l(linea);
        while (!l.empty() && (l.back() == '\n' || l.back() == ' ')) l.pop_back();
        if (!l.empty()) s.insert(l);
      }
      std::fclose(f);
    }
    return s;
  }();
  return abiertas;
}
long g_confirmarDesinstalar = 0;   /* el desinstalador pide dos clics seguidos: hasta cuando vale el primero */
long g_confirmarMako = 0, g_confirmarMakoFg = 0;   /* (v1.18) los de MAKO, igual */
struct Comprobacion {
  pid_t pid = 0;
  std::string parte;
} g_comprobacion;
std::string rutaActualizacion() { return rutaEstadoOdyssey("actualizacion"); }
}  // namespace

bool comprobacionEnMarcha() { return g_comprobacion.pid > 0; }

bool comprobarActualizacion(std::string *err) {
  if (g_comprobacion.pid > 0) return true;
  g_comprobacion.parte = rutaActualizacion() + ".parte";
  ::unlink(g_comprobacion.parte.c_str());
  const pid_t pid = ::fork();
  if (pid < 0) {
    if (err) *err = std::string("no se pudo arrancar la comprobacion: ") + std::strerror(errno);
    return false;
  }
  if (pid == 0) {
    const int nulo = ::open("/dev/null", O_RDWR);
    if (nulo >= 0) { ::dup2(nulo, 0); ::dup2(nulo, 1); ::dup2(nulo, 2); }
    const char *url = std::getenv("ODYSSEYVK_URL_VERSION");   /* taller: otra direccion (vale file://) */
    if (url)
      ::execlp("curl", "curl", "-fsSL", "--max-time", "25", "-o", g_comprobacion.parte.c_str(), url, (char *)nullptr);
    else
      ::execlp("sh", "sh", "-c",
               "curl -fsSL --max-time 20 -H 'Accept: application/vnd.github.raw' -o \"$0\" \"$1\" || "
               "curl -fsSL --max-time 20 -o \"$0\" \"$2\"",
               g_comprobacion.parte.c_str(), kUrlVersionApi, kUrlVersionRemota, (char *)nullptr);
    ::_exit(127);
  }
  g_comprobacion.pid = pid;
  return true;
}

bool vigilarComprobacion() {
  if (g_comprobacion.pid <= 0) return false;
  int estado = 0;
  if (::waitpid(g_comprobacion.pid, &estado, WNOHANG) == 0) return false;
  g_comprobacion.pid = 0;
  std::string txt;
  const bool bien = WIFEXITED(estado) && WEXITSTATUS(estado) == 0 && leeTexto(g_comprobacion.parte, txt);
  ::unlink(g_comprobacion.parte.c_str());
  std::string remota;
  for (char ch : txt)
    if ((ch >= '0' && ch <= '9') || ch == '.') remota += ch;
    else if (!remota.empty()) break;
  if (bien && !remota.empty() && remota.size() < 16) {
    if (FILE *f = std::fopen(rutaActualizacion().c_str(), "we")) {
      std::fprintf(f, "remota = %s\nhora = %ld\n", remota.c_str(), (long)::time(nullptr));
      std::fclose(f);
    }
  }
  return true;
}

Actualizacion estadoActualizacion() {
  Actualizacion a;
  std::string txt;
  if (!leeTexto(rutaActualizacion(), txt)) return a;
  size_t ini = 0;
  while (ini < txt.size()) {
    size_t fin = txt.find('\n', ini);
    if (fin == std::string::npos) fin = txt.size();
    const std::string l = strTrim(txt.substr(ini, fin - ini));
    ini = fin + 1;
    const size_t eq = l.find('=');
    if (eq == std::string::npos) continue;
    const std::string k = strTrim(l.substr(0, eq)), val = strTrim(l.substr(eq + 1));
    if (k == "remota") a.remota = val;
    else if (k == "hora") a.hora = std::atol(val.c_str());
  }
  if (!a.remota.empty()) {
    a.pendiente = versionMasNueva(a.remota, ODYSSEYVK_VERSION);
    a.alDia = !a.pendiente;
  }
  return a;
}

namespace {
/* una orden nuestra en una unidad transitoria de systemd, fuera del servicio: `systemd-run --user` */
bool lanzarAparte(const char *orden, std::string *err) {
  char propio[512] = {0};
  const ssize_t n = ::readlink("/proc/self/exe", propio, sizeof propio - 1);
  const std::string exe = n > 0 ? std::string(propio, (size_t)n) : std::string("odysseyvk");
  const std::string unidad = std::string("odysseyvk-") + orden + "-" + std::to_string((long)::time(nullptr));
  const pid_t pid = ::fork();
  if (pid < 0) {
    if (err) *err = std::string("no se pudo arrancar: ") + std::strerror(errno);
    return false;
  }
  if (pid == 0) {
    const int nulo = ::open("/dev/null", O_RDWR);
    if (nulo >= 0) { ::dup2(nulo, 0); ::dup2(nulo, 1); ::dup2(nulo, 2); }
    ::execlp("systemd-run", "systemd-run", "--user", "--collect", "--quiet", ("--unit=" + unidad).c_str(),
             exe.c_str(), orden, (char *)nullptr);
    ::_exit(127);
  }
  int estado = 0;
  ::waitpid(pid, &estado, 0);
  if (!(WIFEXITED(estado) && WEXITSTATUS(estado) == 0)) {
    if (err) *err = std::string("systemd-run no pudo arrancar `odysseyvk ") + orden + "`";
    return false;
  }
  return true;
}
}  // namespace

bool lanzarActualizacion(std::string *err) { return lanzarAparte("actualizar", err); }
bool lanzarDesinstalacion(std::string *err) { return lanzarAparte("desinstalar", err); }

namespace {
/* La red de profundidad (v1.16): la FP16 (la recomendada), la FP32 y cualquier .onnx que se ponga en la carpeta, cada
   una con su casilla: la puesta va marcada (una sola). La que no esta bajada sale en gris con «Descargar · 50 MB», y
   picarla la baja y la pone; las que estan, con «Descargada» (o «En la carpeta») en azul (Rodolfo: «activalas con un
   toggle, en gris la no descargada y en azul la que si»). Sin ninguna, arriba del todo y con el aviso de para que
   hace falta.
   Mitad y mitad (Rodolfo: «I want a dropdown button for Depth network once the user exceeds 2 models, Add to the right
   side, like half and half one for video only models»): las de siempre a la izquierda y las de solo videos a la
   derecha, y un lado con mas de dos redes es un desplegable. Arriba, en medio, la casilla de la vista de depuracion
   («a debug view on and off button for live testing in the middle top between both sides»): puesta, el panel deja
   solo esto en una columna a la izquierda, las de siempre arriba y las de video abajo, y junto a la casilla lo que
   rinde la red («if there is a way to see the model performance measure it on the left panel mentioned beside the
   debug toggle»). En el panel flotante de antes (ODYSSEYVK_PANEL_VIEJO=1) van a lo ancho y sin la casilla */
struct OpcionRed {
  std::string nombre, etiqueta, id;
  int estado = 0;   /* 0 = esta (azul), 1 = se baja al picarla (gris), 2 = no se puede ahora (otra bajando) */
};

void filasDeLaRed(Hud::State &st, const Valores &v, std::vector<std::string> &ids) {
  const std::vector<RedDisco> redes = redesDelDisco(v), deVideo = redesDeVideo(v);
  const std::string puesta = redPuesta(v);
  const bool dep = st.depuracion && st.marco;
  const int ladoRed = st.marco ? 1 : 0, ladoVideo = st.marco ? 2 : 0;
  const int bajando = descargaCual();
  static const char *const kMb[] = {"50", "99", "117"};   /* lo que pesa cada descarga: la FP16, la FP32, la de video */
  auto pon = [&](Hud::ItemPanel it, const std::string &id) {
    st.items.push_back(std::move(it));
    ids.push_back(id);
  };
  auto nombreDe = [](const RedDisco &r) {
    return r.conocida == 1   ? std::string(idioma::tr("Depth Anything V2 pequeña · FP16 (recomendada)"))
           : r.conocida == 2 ? std::string(idioma::tr("Depth Anything V2 pequeña · FP32"))
           : r.conocida == 3 ? std::string(idioma::tr("Video Depth Anything pequeña"))
                             : r.nombre;
  };
  /* las notas, cortas: caben en media fila */
  auto notaDe = [](const RedDisco &r) {
    if (r.conocida == 1) return std::string(idioma::tr("Igual de buena, un 35 % más deprisa y con 260 MB menos."));
    if (r.conocida == 2) return std::string(idioma::tr("Más lenta en la tarjeta, pero 5 veces más rápida por la CPU."));
    if (r.conocida == 3) return std::string(idioma::tr("ONNX de la comunidad (no hay una oficial) · 117 MB · Apache-2.0"));
    return idioma::trf("{} · {} MB", {r.ruta.rfind(carpetaRedes(), 0) == 0 ? r.ruta.substr(carpetaRedes().size()) : r.ruta,
                                      std::to_string(std::max(1L, r.bytes / 1000000))});
  };
  /* cual se baja: 0 = la FP16, 1 = la FP32, 2 = la de video */
  auto cualDe = [](const RedDisco &r) { return r.conocida == 3 ? 2 : (r.conocida == 2 ? 1 : 0); };
  /* una red con su casilla; `previo` es el principio de su orden («red:» o «redvid:») */
  auto ponCasilla = [&](const RedDisco &r, bool marcada, int lado, const char *previo) {
    Hud::ItemPanel it;
    it.tipo = Hud::ItemPanel::Interruptor, it.lado = lado;
    it.rotulo = nombreDe(r), it.nota = notaDe(r);
    it.valor = r.presente && marcada;
    if (r.presente) {
      it.pastilla = idioma::tr(r.conocida ? "Descargada" : "En la carpeta");
      it.pastillaAcento = true;
      pon(it, previo + r.ruta);
    } else {
      const int cual = cualDe(r);
      it.apagado = true;
      it.descargable = bajando < 0;   /* una descarga a la vez: mientras baja una, la otra no se pica */
      it.pastilla = bajando == cual ? textoDescarga() : idioma::trf("Descargar · {} MB", {kMb[cual]});
      pon(it, "red_bajar:" + std::to_string(cual));
    }
  };
  /* un lado con mas de dos: el desplegable. Su id lleva detras de la cabeza la orden de cada opcion (la de su casilla),
     separadas por \x1f; el panel toma la de la que se pique (panel_ajustes.cpp) */
  auto opcionDe = [&](const RedDisco &r, const char *previo) {
    OpcionRed o;
    o.nombre = nombreDe(r);
    if (r.presente) {
      o.etiqueta = idioma::tr(r.conocida ? "Descargada" : "En la carpeta");
      o.id = previo + r.ruta;
    } else {
      const int cual = cualDe(r);
      o.estado = bajando < 0 ? 1 : 2;
      o.etiqueta = bajando == cual ? textoDescarga() : idioma::trf("Descargar · {} MB", {kMb[cual]});
      o.id = "red_bajar:" + std::to_string(cual);
    }
    return o;
  };
  auto ponDesplegable = [&](const std::string &cabeza, const std::vector<OpcionRed> &ops, int elegido, int lado) {
    Hud::ItemPanel d;
    d.tipo = Hud::ItemPanel::Desplegable, d.lado = lado, d.elegido = elegido;
    std::string id = cabeza;
    for (const OpcionRed &o : ops) {
      d.opciones.push_back(o.nombre), d.ayudas.push_back(o.etiqueta), d.estadosOpc.push_back(o.estado);
      id += '\x1f' + o.id;
    }
    d.valorTexto = elegido >= 0 && (size_t)elegido < ops.size() ? ops[(size_t)elegido].nombre : std::string(idioma::tr("Ninguna"));
    pon(d, id);
  };

  /* 1) la casilla de la vista de depuracion: en medio, encima de las dos mitades. Solo con un video o un juego en 3D
     abierto (es su mapa de profundidad, en vivo); en la vista va arriba de la columna, con lo que rinde la red */
  if (st.marco) {
    const auto hub = ajuste::leerEstadoHub();
    const auto ses = hub.find("sesion");
    const bool abierta = ses != hub.end() && !ses->second.empty() && ses->second != "ninguna";
    Hud::ItemPanel d;
    d.tipo = Hud::ItemPanel::Interruptor;
    d.rotulo = idioma::tr("Vista de depuración");
    d.valor = dep;
    d.centrado = !dep;
    d.apagado = !abierta && !dep;
    d.ayuda = idioma::tr(abierta ? "El mapa de profundidad en vivo (Ctrl+Mayús+9), con las dos listas a un lado: prueba las redes con el vídeo a la vista."
                                 : "Hace falta un vídeo o un juego en 3D abierto: enseña su mapa de profundidad, en vivo.");
    if (!dep) {
      pon(d, "depuracion");
    } else {
      /* lo que cuenta el visor en su `estado` (dos veces por segundo; el panel lo vuelve a leer cada segundo) */
      const auto e = ajuste::leerEstado();
      auto de = [&](const char *k) {
        const auto it = e.find(k);
        return it == e.end() || it->second.empty() ? std::string("-") : it->second;
      };
      auto coma = [](std::string t) {
        for (char &c : t)
          if (c == '.') c = idioma::decimal();
        return t;
      };
      const bool conv = de("conversor") == "puesto";
      const std::string pj = de("prof_juego");
      const bool dlss = conv && pj != "-";
      std::string modelo = de("modelo");
      if (const size_t b = modelo.rfind('/'); b != std::string::npos) modelo = modelo.substr(b + 1);
      if (dlss) {
        d.nota = idioma::tr("sin usar: la profundidad viene de DLSS");
      } else if (conv) {
        d.nota = modelo + " · " + de("tarjeta");
        d.pastilla = idioma::trf("{} ms por mapa", {coma(de("red_ms"))});
        d.pastillaAcento = true;
      } else {
        d.nota = idioma::tr("Sin red ahora: el vídeo dividido ya trae sus dos vistas.");
      }
      pon(d, "depuracion");
      if (conv && !dlss) {
        auto fila = [&](const std::string &que, const std::string &cuanto) {
          Hud::ItemPanel f;
          f.tipo = Hud::ItemPanel::Fila, f.rotulo = que, f.valorTexto = cuanto;
          pon(f, "");
        };
        /* la red sola y con lo de antes y despues (preparar la foto, limpiar y subir el mapa) */
        fila(idioma::tr("Tiempo por mapa"), idioma::trf("{} ms la red · {} ms en total", {coma(de("red_ms")), coma(de("mapa_ms"))}));
        fila(idioma::tr("Mapas por segundo"), coma(de("mapas_s")));
        fila(idioma::tr("Cuadros por segundo"), coma(de("fps")));
      }
    }
  }

  /* 2) las de siempre: a la izquierda (arriba en la vista de depuracion) */
  Hud::ItemPanel s;
  s.tipo = Hud::ItemPanel::Seccion, s.lado = ladoRed;
  s.rotulo = idioma::tr(puesta.empty() ? "FALTA LA RED DE PROFUNDIDAD" : "RED DE PROFUNDIDAD");
  pon(s, "");
  if (puesta.empty() && !dep) {
    Hud::ItemPanel i;
    i.tipo = Hud::ItemPanel::Info, i.lado = ladoRed;
    i.rotulo = idioma::tr("Sin ella no hay datos de profundidad: el vídeo normal no se puede pasar a 3D (el dividido sí).");
    i.nota = idioma::tr("Depth Anything V2 pequeña (huggingface.co/onnx-community): la FP16 es la recomendada.");
    pon(i, "");
  }
  if (redes.size() > 2 && st.marco) {
    std::vector<OpcionRed> ops;
    int elegido = -1;
    for (const RedDisco &r : redes) {
      if (r.presente && r.ruta == puesta) elegido = (int)ops.size();
      ops.push_back(opcionDe(r, "red:"));
    }
    ponDesplegable("lista:red", ops, elegido, ladoRed);
  } else {
    for (const RedDisco &r : redes) ponCasilla(r, r.ruta == puesta, ladoRed, "red:");
  }
  /* (v1.17) donde poner mas, al pie de cada lista (Rodolfo: «lo de video mencionalo para los modelos de la derecha, esto
     esta mal redactado»): la carpeta (con ~, la de cada usuario) en la de la izquierda y lo del nombre en la derecha */
  std::string carpeta = carpetaRedes().substr(0, carpetaRedes().size() - 1);
  if (const char *home = std::getenv("HOME"); home && *home && carpeta.rfind(std::string(home) + "/", 0) == 0)
    carpeta = "~" + carpeta.substr(std::strlen(home));
  if (!dep) {
    Hud::ItemPanel n;
    n.tipo = Hud::ItemPanel::Nota, n.lado = ladoRed;
    n.rotulo = idioma::trf("Las redes .onnx que dejes en {} salen aquí.", {carpeta});
    pon(n, "");
  }

  /* 3) solo para videos (Rodolfo: «can we try this for videos only?»): Video Depth Anything mira los ultimos 32 cuadros a
     la vez. MEDIDO el 27-nov en la RTX 5080 con dos videos a 6 mapas por segundo: menos saltos de cerca y lejos de un
     mapa al siguiente (la mitad en un paisaje), pero 123 ms por mapa contra 4 de la de siempre, y 2,8 GB. Se pone o se
     quita (sin ninguna, los videos van con la de siempre); los juegos, siempre con la de siempre. A la derecha (abajo
     en la vista de depuracion) */
  Hud::ItemPanel sv;
  sv.tipo = Hud::ItemPanel::Seccion, sv.lado = ladoVideo, sv.rotulo = idioma::tr("SOLO PARA VÍDEOS (EXPERIMENTAL)");
  pon(sv, "");
  if (!dep) {
    Hud::ItemPanel nv;
    nv.tipo = Hud::ItemPanel::Info, nv.lado = ladoVideo;
    nv.rotulo = idioma::tr("Mira los últimos 32 cuadros: menos saltos de profundidad. Los juegos no la usan.");
    nv.nota = idioma::tr("Unos 120 ms por mapa y 2,8 GB en una RTX 5080 (no va por la CPU).");
    pon(nv, "");
  }
  if (deVideo.size() > 2 && st.marco) {
    /* la primera, ninguna: los videos con la de siempre */
    std::vector<OpcionRed> ops;
    OpcionRed nada;
    nada.nombre = idioma::tr("Ninguna: los vídeos con la de siempre");
    nada.id = "redvid:";
    ops.push_back(nada);
    int elegido = 0;
    for (const RedDisco &r : deVideo) {
      if (r.presente && r.ruta == v.modeloVideo) elegido = (int)ops.size();
      ops.push_back(opcionDe(r, "redvid:"));
    }
    ponDesplegable("lista:redvid", ops, elegido, ladoVideo);
  } else {
    for (const RedDisco &r : deVideo) ponCasilla(r, r.ruta == v.modeloVideo, ladoVideo, "redvid:");
  }
  if (!dep) {
    Hud::ItemPanel n;
    n.tipo = Hud::ItemPanel::Nota, n.lado = ladoVideo;
    n.rotulo = idioma::tr("Las de esa carpeta con «video» en el nombre salen en esta lista.");
    pon(n, "");
  }
}
/* Rendimiento (v0.86): lo que el visor cuenta en su `estado` (dos veces por segundo) y lo que dice
   nvidia-smi de las tarjetas. Desde la rama experimental va dentro de Programa (Rodolfo: «mete la info de
   rendimiento en la casilla de programa»); se rellena cada segundo mientras esta a la vista. */
void filasRendimiento(Hud::State &st, const Valores &v, std::vector<std::string> &ids) {
  auto pon = [&](Hud::ItemPanel it, const std::string &id) {
    st.items.push_back(std::move(it));
    ids.push_back(id);
  };
  /* Rendimiento (v0.86): lo que el visor cuenta en su `estado` (dos veces por segundo) y lo que
     dice nvidia-smi de las tarjetas. Se rellena de nuevo cada segundo mientras esta a la vista. */
  auto fila = [&](const std::string &que, const std::string &cuanto) {
    Hud::ItemPanel f;
    f.tipo = Hud::ItemPanel::Fila, f.rotulo = que, f.valorTexto = cuanto;
    pon(f, "");
  };
  Hud::ItemPanel s;
  s.tipo = Hud::ItemPanel::Seccion, s.rotulo = idioma::tr("VISOR 3D");
  pon(s, "");
  const auto hub = ajuste::leerEstadoHub();
  const auto ses = hub.find("sesion");
  const bool abierta = ses != hub.end() && !ses->second.empty() && ses->second != "ninguna";
  if (!abierta) {
    Hud::ItemPanel n;
    n.tipo = Hud::ItemPanel::Nota, n.rotulo = idioma::tr("No hay ningún vídeo ni juego en 3D ahora mismo.");
    pon(n, "");
  } else {
    const auto e = ajuste::leerEstado();
    auto de = [&](const char *k) {
      const auto it = e.find(k);
      return it == e.end() ? std::string("-") : it->second;
    };
    std::string fps = de("fps"), ms = de("mapa_ms"), modelo = de("modelo");
    for (char &c : fps) if (c == '.') c = idioma::decimal();
    for (char &c : ms) if (c == '.') c = idioma::decimal();
    const size_t barra = modelo.rfind('/');
    if (barra != std::string::npos) modelo = modelo.substr(barra + 1);
    fila(idioma::tr("Cuadros por segundo"), fps);
    const bool conv = de("conversor") == "puesto";
    fila(idioma::tr("Conversor de relieve"), conv ? idioma::trf("puesto · {} ms por mapa", {ms}) : std::string(idioma::tr("apagado")));
    /* (v1.13) la profundidad de DLSS: si llega, la red no se usa («que aparezca aqui que se esta procesando
       con DLSS», Rodolfo). `prof_juego` es «1920x1080 D32_SFLOAT, Z invertida»: aqui solo la medida */
    std::string pj = de("prof_juego");
    const bool dlss = conv && pj != "-" && !pj.empty();
    if (conv) fila(idioma::tr("Red de profundidad"), dlss ? std::string(idioma::tr("sin usar: la profundidad viene de DLSS"))
                                                          : modelo + " · " + de("tarjeta"));
    if (dlss) {
      const size_t esp = pj.find(' ');
      if (esp != std::string::npos) pj = pj.substr(0, esp);
      const size_t x = pj.find('x');
      if (x != std::string::npos) pj = pj.substr(0, x) + " × " + pj.substr(x + 1);
      auto coma = [](std::string t) {
        for (char &c : t) if (c == '.') c = idioma::decimal();
        return t;
      };
      fila(idioma::tr("Profundidad del juego"), idioma::trf("DLSS · {} (la resolución interna)", {pj}));
      fila(idioma::tr("FPS del juego"), coma(de("fps_juego")));
      fila(idioma::tr("FPS de la capa"), coma(de("fps_capa")));
      const std::string hud = de("hud_juego");
      fila(idioma::tr("HUD del juego"), hud != "-" ? idioma::trf("en el plano de la pantalla · {} % de la imagen", {coma(hud)})
                                                   : std::string(idioma::tr("no se encontró su capa")));
    }
    fila(idioma::tr("Cámara"), de("facetracking") == "abierto" ? de("camara_nota") : std::string(idioma::tr("sin abrir")));
  }
  /* (v1.16) la red de profundidad ya no se repite aqui: va en la pestana 3D, con sus casillas */
  s.rotulo = idioma::tr("TARJETAS GRÁFICAS");
  pon(s, "");
  bool alguna = false;
  if (FILE *f = ::popen("nvidia-smi --query-gpu=name,utilization.gpu,memory.used,memory.total,temperature.gpu "
                        "--format=csv,noheader,nounits 2>/dev/null", "r")) {
    char linea[256];
    while (std::fgets(linea, sizeof linea, f)) {
      std::vector<std::string> c;
      std::string campo;
      for (const char *q = linea; *q; q++) {
        if (*q == ',' || *q == '\n') {
          c.push_back(strTrim(campo));
          campo.clear();
        } else {
          campo += *q;
        }
      }
      if (c.size() < 5) continue;
      std::string nombre = c[0];
      if (nombre.rfind("NVIDIA GeForce ", 0) == 0) nombre = nombre.substr(15);
      char txt[128];
      std::snprintf(txt, sizeof txt, "%s %% · %.1f / %.0f GB · %.0f °C", c[1].c_str(), std::atof(c[2].c_str()) / 1024.0,
                    std::atof(c[3].c_str()) / 1024.0, std::atof(c[4].c_str()));
      std::string t = txt;
      for (char &ch : t) if (ch == '.') ch = idioma::decimal();
      fila(nombre, t);
      alguna = true;
    }
    ::pclose(f);
  }
  /* (v1.16) y las de AMD, de lo que cuenta amdgpu en sysfs (con ROCm la red tambien puede ir en ellas) */
  for (const TarjetaAmd &t : tarjetasAmd()) {
    std::string txt = t.ocupada >= 0 ? std::to_string(t.ocupada) + " %" : std::string("—");
    if (t.usadaMiB >= 0 && t.totalMiB > 0) {
      char b[64];
      std::snprintf(b, sizeof b, " · %.1f / %.0f GB", t.usadaMiB / 1024.0, t.totalMiB / 1024.0);
      txt += b;
    }
    if (t.tempC >= 0.0) {
      char b[32];
      std::snprintf(b, sizeof b, " · %.0f °C", t.tempC);
      txt += b;
    }
    for (char &ch : txt) if (ch == '.') ch = idioma::decimal();
    fila(t.nombre, txt);
    alguna = true;
  }
  if (!alguna) {
    Hud::ItemPanel n;
    n.tipo = Hud::ItemPanel::Nota, n.rotulo = idioma::tr("Ni nvidia-smi ni amdgpu dieron datos de las tarjetas.");
    pon(n, "");
  }
}
}  // namespace

/* ---- la pestana Integraciones (v1.15) ------------------------------------------------------------------------- */
void filasPerfilMako(Hud::State &st, std::vector<std::string> &ids, const std::string &exe, const std::string &deJuego);
namespace {
std::string g_integJuego;     /* el juego cuyas integraciones se ensenan (vacio = las de todos) */
int g_pestanaPedida = -1;
double g_copiadoHasta = 0.0;  /* la linea recien copiada: su boton dice «Copiado» */
struct Edicion {
  std::string id, texto;      /* id = "integ:<clave>" */
  size_t cursor = 0;
  bool todo = false;          /* todo marcado */
} g_edit;

double ahoraSeg() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec + 1e-9 * (double)ts.tv_nsec;
}

std::vector<std::string> idsDe(const std::vector<Juego> &juegos) {
  std::vector<std::string> ids;
  for (const Juego &j : juegos) ids.push_back(j.id);
  return ids;
}

/* lo que se dice en el pie tras guardar un cambio */
std::string dichoDe(const integ::Resultado &r) {
  if (!r.ok) return idioma::trf("guardado, pero no se pudo escribir en Steam: {}", {r.error});
  if (r.escritos == 1) return idioma::tr("guardado y escrito en Steam");
  if (r.escritos > 1) return idioma::trf("guardado y escrito en Steam: {} juegos", {std::to_string(r.escritos)});
  if (r.pendientes > 0 && r.steamAbierto) return idioma::tr("guardado · Steam está abierto: se escribe en cuanto lo cierres");
  return idioma::tr("guardado");
}

/* cambia una clave (en todos o en el juego puesto) y lo lleva a Steam */
bool cambiarIntegracion(const std::string &clave, const std::string &valor, const std::vector<Juego> &juegos,
                        std::string *aviso) {
  const std::vector<std::string> ids = idsDe(juegos);
  integ::Estado e = integ::cargar();
  std::map<std::string, std::string> steam;
  if (integ::leerSteam(steam, nullptr)) integ::sincronizar(e, ids, steam);   /* lo cambiado por fuera, antes */
  integ::fijar(e, g_integJuego, clave, valor);
  std::string err;
  if (!integ::guardar(e, &err)) {
    if (aviso) *aviso = idioma::trf("no se pudo guardar: {}", {err});
    return true;
  }
  if (aviso) *aviso = dichoDe(integ::aplicar(ids, true));
  return true;
}

size_t cpAntes(const std::string &s, size_t i) {
  if (i == 0) return 0;
  i--;
  while (i > 0 && ((unsigned char)s[i] & 0xC0) == 0x80) i--;
  return i;
}
size_t cpDespues(const std::string &s, size_t i) {
  if (i >= s.size()) return s.size();
  i++;
  while (i < s.size() && ((unsigned char)s[i] & 0xC0) == 0x80) i++;
  return i;
}

void filasIntegraciones(Hud::State &st, const std::vector<Juego> &juegos, std::vector<std::string> &ids) {
  auto pon = [&](Hud::ItemPanel it, const std::string &id) {
    st.items.push_back(std::move(it));
    ids.push_back(id);
  };
  const std::vector<std::string> appids = idsDe(juegos);
  std::string nombreJuego;
  for (const Juego &j : juegos)
    if (j.id == g_integJuego) nombreJuego = j.nombre;
  if (nombreJuego.empty()) g_integJuego.clear();
  const std::string &jid = g_integJuego;
  /* lo que hay ahora: integraciones.conf con lo cambiado en Steam por fuera (sin guardar: solo se mira) */
  integ::Estado e = integ::cargar();
  std::map<std::string, std::string> steam;
  std::string errSteam;
  const bool haySteam = integ::leerSteam(steam, &errSteam);
  if (haySteam) integ::sincronizar(e, appids, steam);
  const std::map<std::string, std::string> pend = haySteam ? integ::porEscribir(e, appids, steam) : std::map<std::string, std::string>{};
  const integ::Ajustes ef = integ::efectivo(e, jid);
  const auto jj = jid.empty() ? e.juegos.end() : e.juegos.find(jid);
  const bool hayJuego = jj != e.juegos.end();
  auto propio = [&](const char *k) { return hayJuego && jj->second.propios.count(k) > 0; };
  auto val = [&](const char *k) {
    const auto it = ef.find(k);
    return it == ef.end() ? std::string() : it->second;
  };
  auto interruptor = [&](const char *clave, const std::string &rotulo, const std::string &ayuda) {
    Hud::ItemPanel it;
    it.tipo = Hud::ItemPanel::Interruptor, it.rotulo = rotulo, it.valor = val(clave) == "1", it.ayuda = ayuda;
    it.propio = propio(clave);
    /* (v1.15) lo que no esta en el equipo, en gris (Rodolfo: «que la pill diga Proyecto no instalado», «o lo que
       supongas mejor»); y lo que es uno u otro, en gris mientras el otro este puesto */
    const char *otra = integ::excluyente(clave);
    if (!integ::instalado(clave)) {
      it.apagado = true, it.pastilla = idioma::tr("No instalada");
      it.ayuda = std::string(idioma::tr("Esta herramienta no está instalada en el equipo")) + (ayuda.empty() ? "" : " · " + ayuda);
    } else if (otra[0] && !it.valor && val(otra) == "1") {
      it.apagado = true;
      it.nota = idioma::trf("Es una u otra: está puesto {}", {std::strcmp(otra, "vkdaedalus") == 0 ? "ENABLE_VKDAEDALUS" : "vkdaedalus-run"});
    }
    pon(it, std::string("integ:") + clave);
    return st.items.size() - 1;
  };
  auto campo = [&](const char *clave, const std::string &rotulo, const char *ejemplo, const std::string &ayuda) {
    Hud::ItemPanel it;
    it.tipo = Hud::ItemPanel::Texto, it.rotulo = rotulo, it.nota = ejemplo, it.ayuda = ayuda;
    it.propio = propio(clave);
    const std::string id = std::string("integ:") + clave;
    if (g_edit.id == id) it.valorTexto = g_edit.texto, it.cursor = (int)g_edit.cursor, it.seleccion = g_edit.todo;
    else it.valorTexto = val(clave);
    pon(it, id);
  };
  Hud::ItemPanel sec;
  sec.tipo = Hud::ItemPanel::Seccion;

  /* de un juego: su nombre arriba y el boton de volver a los de todos */
  if (!jid.empty()) {
    Hud::ItemPanel b;
    b.tipo = Hud::ItemPanel::Boton, b.rotulo = nombreJuego;
    if (!(hayJuego && jj->second.propia))   /* la que no se sabe leer lo dice abajo */
      b.nota = hayJuego && !jj->second.propios.empty() ? idioma::tr("Lo marcado con un punto es solo de este juego; lo demás, como todos los juegos")
                                                       : idioma::tr("Como todos los juegos: lo que cambies aquí será solo de este");
    b.valorTexto = idioma::tr("Todos los juegos"), b.valor = true;
    pon(b, "integ:volver");
    if (hayJuego && jj->second.propia) {
      Hud::ItemPanel in;
      in.tipo = Hud::ItemPanel::Info, in.rotulo = idioma::tr("Su línea de Steam no la sé leer, así que OdysseyVK no la toca");
      in.nota = idioma::tr("Lleva algo que no se puede poner por piezas (un ;, una tubería, una redirección...).");
      pon(in, "");
      Hud::ItemPanel l;
      l.tipo = Hud::ItemPanel::Linea, l.valorTexto = steam.count(jid) ? steam.at(jid) : std::string();
      pon(l, "");
      Hud::ItemPanel u;
      u.tipo = Hud::ItemPanel::Boton, u.rotulo = idioma::tr("Usar la de OdysseyVK");
      u.nota = idioma::tr("Se cambia por la que salga de aquí (antes se guarda una copia de la configuración de Steam)");
      u.valorTexto = idioma::tr("Usar"), u.valor = true;
      pon(u, "integ:usar");
      return;
    }
  }

  /* PARAMETROS: las variables de cada capa; apagadas, sin el =1 (Rodolfo: «les quitas el =1 si no lo activan») */
  sec.rotulo = idioma::tr("PARÁMETROS");
  pon(sec, "");
  struct Var {
    const char *clave, *nombre, *ayuda;
  };
  /* (v1.16) ODYSSEYVK otra vez con su interruptor, como las demas (Rodolfo: «return it as an option») */
  const Var vars[] = {{"odysseyvk", "ODYSSEYVK", "La capa de OdysseyVK: en los juegos con DLSS, el 3D usa la profundidad del juego"},
                      {"mangohud", "MANGOHUD", "El contador de MangoHud encima del juego"},
                      {"vkdaedalus", "ENABLE_VKDAEDALUS", "La capa de vkDaedalus"},
                      {"neural_forge", "NEURAL_FORGE_ENABLE", "La capa de Neural Forge; su ejecutable (NEURAL_FORGE_TARGET_EXE) se toma de Neural Forge"},
                      {"vkbasalt", "ENABLE_VKBASALT", "Los filtros de vkBasalt"},
                      {"mako", "ENABLE_MAKO", "La generación de cuadros de MAKO: se ajusta con clic derecho en el cuadrito de los FPS"}};
  for (const Var &v : vars) {
    const bool puesto = val(v.clave) == "1";
    const size_t k = interruptor(v.clave, std::string(v.nombre) + (puesto ? "=1" : ""), idioma::tr(v.ayuda));
    /* (v1.18) apagado aqui porque el juego se cayo con el (odysseyvk juego): sale apagado, con la hora */
    if (!jid.empty() && puesto && std::strcmp(v.clave, "neural_forge") == 0 &&
        fileExists(rutaEstadoOdyssey(("nf-apagado-" + jid).c_str()))) {
      std::string txt;
      readSmallFile(rutaEstadoOdyssey(("nf-apagado-" + jid).c_str()), txt);
      long hora = 0;
      if (const size_t h = txt.find("hora = "); h != std::string::npos) hora = std::atol(txt.c_str() + h + 7);
      char cuando[32] = "";
      if (hora > 0) {
        const time_t tt = (time_t)hora;
        std::strftime(cuando, sizeof cuando, "%d/%m %H:%M", std::localtime(&tt));
      }
      st.items[k].valor = false;
      st.items[k].nota = idioma::trf("Apagado aquí: el juego se cerró con él por un fallo de la tarjeta ({}). Ponlo para probar otra vez.", {cuando});
      continue;
    }
    /* de un juego: el ejecutable que va con Neural Forge (no se pregunta: se saca de Neural Forge) */
    if (!jid.empty() && puesto && std::strcmp(v.clave, "neural_forge") == 0) {
      const std::string exe = integ::exeDe(e, jid);
      if (!exe.empty()) st.items[k].nota = "NEURAL_FORGE_TARGET_EXE=" + exe;
      /* (v1.18) sin el ejecutable, Neural Forge no sabe a que proceso del juego entrar (Rodolfo: «needs the game added to
         their list»): eso lo hace su pestana Setup */
      else st.items[k].nota = idioma::tr("No está en la lista de Neural Forge: añádelo en su pestaña Setup");
    }
    /* (v1.18) con Neural Forge puesto, su modelo en la segunda tarjeta: NEURAL_FORGE_OFFLOAD_GPU con la tarjeta y su enlace
       (Rodolfo: «remember to consider some users use x8»; ver integ::secundariaNf) */
    if (puesto && std::strcmp(v.clave, "neural_forge") == 0) {
      integ::Secundaria s;
      const bool hay = integ::secundariaNf(s, val("juego_tarjeta"));
      const bool offload = val("nf_offload") == "1";
      const std::string propia = jid.empty() ? std::string() : integ::gpuDe(e, jid);
      const std::string g = !propia.empty() ? propia : (hay ? s.valor() : std::string());
      const size_t ko = interruptor("nf_offload", std::string("NEURAL_FORGE_OFFLOAD_GPU") + (offload && !g.empty() ? "=" + g : ""),
                                    idioma::tr("El modelo de Neural Forge en la segunda tarjeta (su offloading), no en la del juego. Cada cuadro cruza el PCIe hasta ella y vuelve: con x4 o x8 puede ir más lento que en la tarjeta del juego. Neural Forge pide una NVIDIA RTX 40 o más nueva."));
      if (hay) {
        std::string nota = s.nombre + " · PCIe " + std::to_string(s.gen) + ".0 x" + std::to_string(s.ancho);
        if ((s.ancho > 0 && s.ancho < 16) || (s.gen > 0 && s.gen < 4))
          nota += " · " + std::string(idioma::tr("cada cuadro va y vuelve por ahí: puede ir más lento que en la tarjeta del juego"));
        /* (v1.18) con el juego en otra tarjeta que la del monitor, la segunda de Neural Forge es la del monitor: por ahi ya
           cruza cada cuadro del juego (Neural Forge pide el monitor en la tarjeta del juego) */
        if (s.delMonitor) nota += " · " + std::string(idioma::tr("también da el monitor 3D: cada cuadro del juego ya cruza hasta ella"));
        st.items[ko].nota = nota;
      } else if (integ::instalado("neural_forge")) {
        st.items[ko].pastilla = idioma::tr("Sin segunda tarjeta");
        st.items[ko].ayuda = idioma::tr("No hay una segunda tarjeta NVIDIA: el modelo se queda en la del juego");
      }
    }
  }
  /* (v1.18) la generacion de cuadros de ESTE juego: su perfil de MAKO, por su ejecutable (el que apunto el visor al verlo, o
     el de la lista de Neural Forge) */
  if (!jid.empty() && mako::instalado()) {
    Hud::ItemPanel sfg;
    sfg.tipo = Hud::ItemPanel::Seccion, sfg.rotulo = idioma::tr("GENERACIÓN DE CUADROS");
    pon(sfg, "");
    const std::string exe = mako::exeDeJuego(jid, integ::exeDe(e, jid));
    Hud::ItemPanel ne;
    ne.tipo = Hud::ItemPanel::Nota;
    if (exe.empty()) {
      ne.rotulo = idioma::tr("Ábrelo una vez con el 3D para saber su ejecutable (el perfil de MAKO va por él).");
      pon(ne, "");
    } else {
      ne.rotulo = exe + (val("mako") == "1" ? std::string() : " · " + std::string(idioma::tr("ENABLE_MAKO está apagado: se guarda para cuando lo pongas")));
      pon(ne, "");
      filasPerfilMako(st, ids, exe, jid);
    }
  }
  /* (v1.18) la tarjeta del juego, con el monitor 3D en otra (Rodolfo: «haz una opcion de offload con el monitor conectado a
     otra tarjeta tambien»; ver integ::tarjetasParaJuego): con dos o mas dedicadas */
  if (const std::vector<integ::TarjetaJuego> &tjs = integ::tarjetasParaJuego(); tjs.size() > 1) {
    Hud::ItemPanel tj;
    tj.tipo = Hud::ItemPanel::Opciones, tj.rotulo = idioma::tr("Tarjeta del juego"), tj.elegido = 0, tj.propio = propio("juego_tarjeta");
    const std::string pj = val("juego_tarjeta");
    const integ::TarjetaJuego *monitor = nullptr, *elegida = nullptr;
    auto enlaceDe = [](const integ::TarjetaJuego &x) { return "PCIe " + std::to_string(x.gen) + ".0 x" + std::to_string(x.ancho); };
    for (const integ::TarjetaJuego &x : tjs)
      if (x.delMonitor) monitor = &x;
    for (size_t i = 0; i < tjs.size(); i++) {
      const integ::TarjetaJuego &x = tjs[i];
      tj.opciones.push_back(x.nombre);
      tj.ayudas.push_back(x.delMonitor ? std::string(idioma::tr("La del monitor 3D: el juego y el 3D en la misma tarjeta, sin copias"))
                                       : idioma::trf("El juego en la {}: cada cuadro cruza el PCIe ({}) hasta la del monitor 3D", {x.nombre, enlaceDe(x)}));
      if ((pj.empty() && x.predeterminada) || x.pci == pj) tj.elegido = (int)i, elegida = &x;
    }
    pon(tj, "integ:juego_tarjeta");
    /* las filas de opciones no ensenan nota: lo que cuesta va en una nota debajo, solo con el juego en otra tarjeta */
    if (elegida && !elegida->delMonitor && monitor) {
      /* lo que manda es el enlace mas lento de las dos */
      const long a = (long)elegida->ancho << elegida->gen, b = (long)monitor->ancho << monitor->gen;
      const integ::TarjetaJuego &lento = a <= b ? *elegida : *monitor;
      Hud::ItemPanel n;
      n.tipo = Hud::ItemPanel::Nota;
      n.rotulo = idioma::trf("{} dibuja el juego y {} lo enseña: cada cuadro cruza por {}", {elegida->nombre, monitor->nombre, enlaceDe(lento)});
      pon(n, "");
    }
  }
  campo("otros", idioma::tr("Otros parámetros"), "DXVK_HUD=fps PROTON_LOG=1",
        idioma::tr("Más variables, separadas por espacios: van con las de arriba"));

  /* COMANDO: lo de delante del juego y lo de detras */
  sec.rotulo = idioma::tr("COMANDO");
  pon(sec, "");
  interruptor("vkdaedalus_run", "vkdaedalus-run", idioma::tr("Arranca el juego a través de vkdaedalus-run"));
  campo("comando", idioma::tr("Comando personalizado"), "gamemoderun",
        idioma::tr("Lo que quieras correr delante del juego (va justo antes de %command%)"));
  campo("opciones", idioma::tr("Opciones del juego"), "--launcher-skip", idioma::tr("Van detrás de %command%: se le pasan al juego"));

  /* GAMESCOPE (Rodolfo: «no menciones de que es, solo da a elegir la resolucion de salida y la de renderizado, solo
     las abreviaturas»; y el reescalado, que enciende gamescope, con FSR y NIS) */
  sec.rotulo = "GAMESCOPE";
  pon(sec, "");
  interruptor("gamescope", "Gamescope", "");
  interruptor("reescalado", idioma::tr("Reescalado"), idioma::tr("Al ponerlo se pone gamescope"));
  if (val("reescalado") == "1") {
    Hud::ItemPanel f;
    f.tipo = Hud::ItemPanel::Opciones, f.rotulo = idioma::tr("Filtro"), f.opciones = {"FSR", "NIS"};
    f.ayudas = {"AMD FidelityFX Super Resolution 1.0 · -F fsr", "NVIDIA Image Scaling · -F nis"};
    f.elegido = val("filtro") == "nis" ? 1 : 0, f.propio = propio("filtro") || propio("reescalado");
    pon(f, "integ:filtro");
  }
  if (val("gamescope") == "1") {
    auto resolucion = [&](const char *clave, const std::string &rotulo, const char *banderas) {
      Hud::ItemPanel r;
      r.tipo = Hud::ItemPanel::Opciones, r.rotulo = rotulo, r.elegido = -1, r.propio = propio(clave);
      const std::string puesta = val(clave);
      for (const integ::Resolucion &x : integ::resoluciones()) {
        if (puesta == x.medida) r.elegido = (int)r.opciones.size();
        r.opciones.push_back(x.abrev);
        std::string m = x.medida;
        m.replace(m.find('x'), 1, " × ");
        r.ayudas.push_back(m + " · " + banderas);
      }
      if (r.elegido < 0 && !puesta.empty()) {   /* una que no es de las de siempre (venia en su linea) */
        r.elegido = (int)r.opciones.size();
        r.opciones.push_back(puesta);
        r.ayudas.push_back(banderas);
      }
      pon(r, std::string("integ:") + clave);
    };
    resolucion("render", idioma::tr("Resolución de renderizado"), "-w -h");
    resolucion("salida", idioma::tr("Resolución de salida"), "-W -H");
    Hud::ItemPanel es;
    es.tipo = Hud::ItemPanel::Opciones, es.rotulo = idioma::tr("Escalado"), es.opciones = {"stretch", "fit"};
    es.ayudas = {idioma::tr("Estira la imagen a la pantalla entera · -S stretch"), idioma::tr("La encaja sin deformarla · -S fit")};
    const std::string esc = val("escala");
    es.elegido = esc == "stretch" ? 0 : (esc == "fit" ? 1 : -1);
    if (es.elegido < 0 && !esc.empty()) es.elegido = 2, es.opciones.push_back(esc), es.ayudas.push_back("-S " + esc);
    es.propio = propio("escala");
    pon(es, "integ:escala");
    interruptor("pantalla_completa", idioma::tr("Pantalla completa"), "-f");
    interruptor("cursor", idioma::tr("Atrapar el cursor"), "--force-grab-cursor");
    if (!val("gs_otros").empty() || g_edit.id == "integ:gs_otros")
      campo("gs_otros", idioma::tr("Otras opciones de gamescope"), "-r 60", idioma::tr("Las que traía su línea y no tienen fila aquí"));
  }

  /* LA LINEA: la que va a Steam, con su boton Copiar, y si falta escribirla */
  sec.rotulo = idioma::tr("LÍNEA DE STEAM");
  pon(sec, "");
  {
    Hud::ItemPanel l;
    l.tipo = Hud::ItemPanel::Linea, l.valorTexto = integ::linea(ef, jid.empty() ? std::string() : integ::exeDe(e, jid), jid.empty() ? std::string() : integ::gpuDe(e, jid));
    l.rotulo = idioma::tr("Nada: Steam arranca el juego tal cual");
    l.valor = ahoraSeg() < g_copiadoHasta;
    l.nota = idioma::tr(l.valor ? "Copiado" : "Copiar");
    l.ayuda = jid.empty() ? idioma::tr("La de los juegos sin nada propio. Con Steam abierto puedes pegarla tú: Propiedades del juego → General → Opciones de lanzamiento.")
                          : idioma::tr("Con Steam abierto puedes pegarla tú: Propiedades del juego → General → Opciones de lanzamiento.");
    if (l.valorTexto.empty()) l.nota.clear();
    pon(l, "integ:copiar");
  }
  {
    Hud::ItemPanel b;
    b.tipo = Hud::ItemPanel::Boton;
    const int n = (int)pend.size();
    const bool abierto = integ::steamAbierto();
    if (!haySteam) {
      b.rotulo = idioma::tr("No encontré la configuración de Steam");
      b.nota = errSteam, b.valorTexto = idioma::tr("Escribir"), b.valor = false;
    } else if (n == 0) {
      b.rotulo = idioma::tr("Steam está al día");
      b.nota = idioma::tr("Cada juego tiene en Steam la línea que sale de aquí");
      b.valorTexto = idioma::tr("Escribir"), b.valor = false;
    } else {
      b.rotulo = n == 1 ? std::string(idioma::tr("1 juego con la línea por escribir"))
                        : idioma::trf("{} juegos con la línea por escribir", {std::to_string(n)});
      if (!abierto) {
        b.nota = idioma::tr("Steam está cerrado: se escribe ya (antes se guarda una copia de su configuración)");
        b.valorTexto = idioma::tr("Escribir ahora"), b.valor = true;
      } else if (integ::hayMarca()) {
        b.nota = idioma::tr("Steam está abierto: se escribe solo en cuanto lo cierres");
        b.valorTexto = idioma::tr("Esperando a Steam"), b.valor = false;
      } else {
        b.nota = idioma::tr("Steam está abierto: solo lee esto al arrancar y lo reescribe al salir");
        b.valorTexto = idioma::tr("Al cerrar Steam"), b.valor = true;
      }
    }
    pon(b, "integ:escribir");
  }

  if (!jid.empty()) {
    if (hayJuego && !jj->second.propios.empty()) {
      Hud::ItemPanel v;
      v.tipo = Hud::ItemPanel::Boton, v.rotulo = idioma::tr("Volver a lo de todos los juegos");
      v.nota = idioma::tr("Este juego deja lo suyo y toma lo de todos");
      v.valorTexto = idioma::tr("Volver"), v.valor = true;
      pon(v, "integ:como_todos");
    }
    return;
  }
  /* de todos: los juegos que llevan algo suyo, para ir a ellos (o se abren con el engranaje de su caratula) */
  sec.rotulo = idioma::tr("JUEGOS CON LO SUYO");
  pon(sec, "");
  int cuantos = 0;
  for (const Juego &j : juegos) {
    const auto it = e.juegos.find(j.id);
    if (it == e.juegos.end() || (it->second.propios.empty() && !it->second.propia)) continue;
    cuantos++;
    Hud::ItemPanel b;
    b.tipo = Hud::ItemPanel::Boton, b.rotulo = j.nombre;
    b.nota = it->second.propia ? std::string(idioma::tr("su línea de Steam no la sé leer: no la toco"))
                               : integ::linea(integ::efectivo(e, j.id), integ::exeDe(e, j.id), integ::gpuDe(e, j.id));
    if (b.nota.empty()) b.nota = idioma::tr("Nada: Steam arranca el juego tal cual");
    b.valorTexto = idioma::tr("Abrir"), b.valor = true;
    pon(b, "integ:juego:" + j.id);
  }
  if (cuantos == 0) {
    Hud::ItemPanel n;
    n.tipo = Hud::ItemPanel::Nota, n.rotulo = idioma::tr("Ninguno: todos toman lo de aquí. En Juegos, el engranaje de cada carátula abre lo de ese juego.");
    pon(n, "");
  }
}

/* un golpe en una fila de Integraciones ("integ:<que>") */
bool aplicarIntegracion(const std::string &que, int sub, const std::vector<Juego> &juegos, std::string *aviso) {
  const std::vector<std::string> ids = idsDe(juegos);
  if (que == "volver") {
    g_integJuego.clear();
    g_pestanaPedida = 2;   /* arriba del todo */
    return true;
  }
  if (que.rfind("juego:", 0) == 0) {
    g_integJuego = que.substr(6);
    g_pestanaPedida = 2;
    return true;
  }
  integ::Estado e = integ::cargar();
  std::map<std::string, std::string> steam;
  if (integ::leerSteam(steam, nullptr)) integ::sincronizar(e, ids, steam);
  const integ::Ajustes ef = integ::efectivo(e, g_integJuego);
  if (que == "copiar") {
    const std::string l = integ::linea(ef, g_integJuego.empty() ? std::string() : integ::exeDe(e, g_integJuego),
                                     g_integJuego.empty() ? std::string() : integ::gpuDe(e, g_integJuego));
    std::string err;
    if (l.empty()) return false;
    if (copiarAlPortapapeles(l, &err)) {
      g_copiadoHasta = ahoraSeg() + 2.5;
      if (aviso) *aviso = idioma::tr("copiada: pégala en Steam, en Propiedades → General → Opciones de lanzamiento");
    } else if (aviso) {
      *aviso = idioma::trf("no se pudo copiar ({}): escríbelo a mano", {err});
    }
    return true;
  }
  if (que == "escribir") {
    const integ::Resultado r = integ::aplicar(ids, true);
    if (aviso) {
      if (!r.ok) *aviso = idioma::trf("no se pudo escribir en Steam: {}", {r.error});
      else if (r.escritos > 0) *aviso = idioma::trf("escrito en Steam: {} juegos (hay copia de antes en ~/.local/state/odysseyvk/steam)", {std::to_string(r.escritos)});
      else if (r.pendientes > 0) *aviso = idioma::tr("se escribe en cuanto cierres Steam");
      else *aviso = idioma::tr("Steam ya estaba al día");
    }
    return true;
  }
  if (que == "como_todos" || que == "usar") {
    if (g_integJuego.empty()) return false;
    integ::comoTodos(e, g_integJuego);
    std::string err;
    if (!integ::guardar(e, &err)) {
      if (aviso) *aviso = idioma::trf("no se pudo guardar: {}", {err});
      return true;
    }
    if (aviso) *aviso = dichoDe(integ::aplicar(ids, true));
    return true;
  }
  const auto fab = integ::deFabrica().find(que);
  if (fab == integ::deFabrica().end()) return false;
  const std::string ahora = ef.count(que) ? ef.at(que) : fab->second;
  /* los campos: un clic pone a escribir */
  if (que == "otros" || que == "comando" || que == "opciones" || que == "gs_otros") {
    g_edit = Edicion{};
    g_edit.id = "integ:" + que, g_edit.texto = ahora, g_edit.cursor = ahora.size();
    if (aviso) *aviso = idioma::tr("escribe y pulsa Intro para guardarlo (Esc lo deja como estaba)");
    return true;
  }
  std::string nuevo;
  /* (v1.18) Neural Forge apagado en un juego tras caerse con el (odysseyvk juego): el interruptor sale apagado y picarlo lo
     vuelve a poner (quita la marca) */
  if (que == "neural_forge" && !g_integJuego.empty() && fileExists(rutaEstadoOdyssey(("nf-apagado-" + g_integJuego).c_str()))) {
    ::unlink(rutaEstadoOdyssey(("nf-apagado-" + g_integJuego).c_str()).c_str());
    if (aviso) *aviso = idioma::tr("Neural Forge vuelve a este juego desde la próxima vez que lo abras");
    if (ahora != "1") return cambiarIntegracion(que, "1", juegos, aviso);
    return true;
  }
  if (integ::esInterruptor(que)) {
    nuevo = ahora == "1" ? "0" : "1";
    /* (v1.15) lo de gris no se enciende: lo que no esta instalado, o con su otra puesta */
    const char *otra = integ::excluyente(que);
    if (nuevo == "1" && (!integ::instalado(que) || (otra[0] && ef.count(otra) && ef.at(otra) == "1"))) return false;
  } else if (que == "filtro") {
    nuevo = sub == 1 ? "nis" : "fsr";
  } else if (que == "render" || que == "salida") {
    const auto &rs = integ::resoluciones();
    if (sub < 0 || (size_t)sub >= rs.size()) return false;   /* la que venia en su linea: ya esta puesta */
    nuevo = rs[(size_t)sub].medida;
  } else if (que == "juego_tarjeta") {   /* (v1.18) la predeterminada (donde lo pone Proton) es vacio */
    const std::vector<integ::TarjetaJuego> &tjs = integ::tarjetasParaJuego();
    if (sub < 0 || (size_t)sub >= tjs.size()) return false;
    nuevo = tjs[(size_t)sub].predeterminada ? std::string() : tjs[(size_t)sub].pci;
  } else if (que == "escala") {
    if (sub == 0) nuevo = "stretch";
    else if (sub == 1) nuevo = "fit";
    else return false;
  } else {
    return false;
  }
  if (nuevo == ahora) return false;
  return cambiarIntegracion(que, nuevo, juegos, aviso);
}
}  // namespace

bool editandoTexto() { return !g_edit.id.empty(); }
const std::string &idEditando() { return g_edit.id; }
void cancelarTexto() { g_edit = Edicion{}; }

bool terminarTexto(const std::vector<Juego> &juegos, std::string *aviso) {
  if (g_edit.id.empty()) return false;
  const Edicion ed = g_edit;
  g_edit = Edicion{};
  const std::string clave = ed.id.substr(6);
  integ::Estado e = integ::cargar();
  const integ::Ajustes ef = integ::efectivo(e, g_integJuego);
  std::string t = strTrim(ed.texto);
  if (ef.count(clave) && ef.at(clave) == t) {
    if (aviso) *aviso = idioma::tr("sin cambios");
    return true;
  }
  return cambiarIntegracion(clave, t, juegos, aviso);
}

bool teclaTexto(const std::string &combo, const std::string &texto, const std::vector<Juego> &juegos, std::string *aviso) {
  Edicion &ed = g_edit;
  if (ed.id.empty()) return false;
  if (combo == "Escape") {
    cancelarTexto();
    if (aviso) *aviso = idioma::tr("sin cambios");
    return true;
  }
  if (combo == "Return" || combo == "Tab" || combo == "Shift+Tab") return terminarTexto(juegos, aviso);
  auto insertar = [&](std::string t) {
    for (char &c : t)
      if (c == '\n' || c == '\r' || c == '\t') c = ' ';
    if (ed.todo) ed.texto.clear(), ed.cursor = 0, ed.todo = false;
    if (ed.texto.size() + t.size() > 1000) return;
    ed.texto.insert(ed.cursor, t);
    ed.cursor += t.size();
  };
  if (combo == "Ctrl+A") {
    ed.todo = !ed.texto.empty();
    ed.cursor = ed.texto.size();
    return true;
  }
  if (combo == "Ctrl+V" || combo == "Shift+Insert") {
    std::string p;
    if (!leerPortapapeles(p)) {
      if (aviso) *aviso = idioma::tr("no pude leer el portapapeles");
      return true;
    }
    insertar(strTrim(p));
    return true;
  }
  if (!texto.empty()) {
    insertar(texto);
    return true;
  }
  if (combo == "Backspace" || combo == "Delete") {
    if (ed.todo) {
      ed.texto.clear(), ed.cursor = 0, ed.todo = false;
    } else if (combo == "Backspace" && ed.cursor > 0) {
      const size_t a = cpAntes(ed.texto, ed.cursor);
      ed.texto.erase(a, ed.cursor - a);
      ed.cursor = a;
    } else if (combo == "Delete" && ed.cursor < ed.texto.size()) {
      ed.texto.erase(ed.cursor, cpDespues(ed.texto, ed.cursor) - ed.cursor);
    }
    return true;
  }
  if (combo == "Left" || combo == "Right" || combo == "Home" || combo == "End") {
    if (combo == "Left") ed.cursor = ed.todo ? 0 : cpAntes(ed.texto, ed.cursor);
    else if (combo == "Right") ed.cursor = ed.todo ? ed.texto.size() : cpDespues(ed.texto, ed.cursor);
    else ed.cursor = combo == "Home" ? 0 : ed.texto.size();
    ed.todo = false;
    return true;
  }
  return false;
}

int tomarPestana() {
  const int p = g_pestanaPedida;
  g_pestanaPedida = -1;
  return p;
}

void integracionesDe(const std::string &appid) { g_integJuego = appid; }

/* (v1.18) lo de las fuentes de efectos: cambia mientras se comprueba o se baja (el panel rehace la pestana) */
std::string firmaEfectos() {
  std::string f;
  for (const char *n : {"efectos-fuentes", "efectos-bajada", "efectos-comprobando"}) {
    struct stat sb {};
    if (::stat(rutaEstadoOdyssey(n).c_str(), &sb) == 0) f += std::to_string((long long)sb.st_mtim.tv_sec) + "." + std::to_string(sb.st_mtim.tv_nsec);
    f += "|";
  }
  return f + (efectos::comprobando() ? "c" : "") + (efectos::leerBajada().enMarcha ? "b" : "");
}

std::string firmaIntegraciones() {
  /* (v1.18) y la instalacion de MAKO FG: lo que lleva (su fichero cambia mientras baja) y si ya esta */
  std::string mfg;
  struct stat sb {};
  if (::stat(rutaEstadoOdyssey("mako-fg").c_str(), &sb) == 0) mfg = std::to_string((long long)sb.st_mtim.tv_sec) + "." + std::to_string(sb.st_mtim.tv_nsec);
  return integ::firma() + (ahoraSeg() < g_copiadoHasta ? "|copiado" : "") + "|" + mfg + (mako::hostPropio().empty() ? "" : "|fg");
}

void probarEdicion(const std::string &clave, const std::string &texto) {
  g_edit = Edicion{};
  g_edit.id = "integ:" + clave, g_edit.texto = texto, g_edit.cursor = texto.size();
}

/* ---- (v1.18) la pestana FG: la generacion de cuadros de MAKO ----
   Rodolfo: «quiero una pestaña de framegen aqui arriba», «que diga FG y en base a la activa cambie a X2 X3 y X4», «que dentro
   de su menu se instalen» y «puedes poner solo mako?». La tesela dice el multiplicador del juego que corre; dentro, MAKO, su
   generador (MAKO FG, con su boton de instalar) y el perfil del juego, como el menu del clic derecho en el cuadrito de los
   FPS. El juego sale del `estado` del visor (fg_exe) */
namespace {
struct JuegoFg {
  std::string exe;
  bool enJuego = false;
};
JuegoFg juegoFg() {
  JuegoFg j;
  const auto e = ajuste::leerEstado();
  if (const auto it = e.find("fg_exe"); it != e.end() && it->second != "-") j.exe = it->second;
  if (const auto it = e.find("fg_en_juego"); it != e.end()) j.enJuego = it->second == "si";
  return j;
}
}  // namespace

/* (v1.18) la de los videos, en serve.conf: `fg_video_activo` (si/no, el interruptor «Activar en vídeos») y `fg_video`, el
   multiplicador de 2 a 4 (fgvideo.h). Devuelve el multiplicador si esta encendida, si no 0 */
int fgVideoMultiplicador() {
  const auto sc = ajuste::leerServeConf();
  const auto it = sc.find("fg_video");
  const int m = it == sc.end() ? 0 : std::atoi(it->second.c_str());
  return m >= 2 && m <= 4 ? m : 2;
}
int fgVideoPuesta() {
  const auto sc = ajuste::leerServeConf();
  const auto it = sc.find("fg_video");
  const auto ia = sc.find("fg_video_activo");
  const int m = it == sc.end() ? 0 : std::atoi(it->second.c_str());
  const bool activa = ia != sc.end() ? ia->second == "si" : (m >= 2 && m <= 4);
  return activa ? fgVideoMultiplicador() : 0;
}

std::string notaFg() {
  const JuegoFg j = juegoFg();
  /* sin juego, la de los videos si esta puesta */
  if (j.exe.empty() && fgVideoPuesta() >= 2) return idioma::trf("Vídeos X{}", {std::to_string(fgVideoPuesta())});
  if (!mako::instalado()) return idioma::tr("No instalada");
  if (j.exe.empty()) return idioma::tr("Sin juego");
  const mako::Perfil p = mako::leer(j.exe);
  if (!p.existe || !p.puesta) return idioma::tr("Apagada");
  return "X" + std::to_string(p.multiplicador);
}

/* (v1.18) las filas del perfil de MAKO de un juego (por su ejecutable): las de la pestana FG (el que corre, ids «fg:que») y
   las de las Integraciones de cada juego (Rodolfo: «remember generation preferences per game and include them in their
   parameter list»; ids «fgj:que:exe»). El perfil es de MAKO (su conf.toml), asi que se recuerda por juego solo */
void filasPerfilMako(Hud::State &st, std::vector<std::string> &ids, const std::string &exe, const std::string &deJuego) {
  auto pon = [&](Hud::ItemPanel it, const std::string &id) {
    st.items.push_back(std::move(it));
    ids.push_back(id);
  };
  auto idDe = [&](const char *que) { return deJuego.empty() ? std::string("fg:") + que : std::string("fgj:") + que + ":" + exe; };
  const mako::Perfil p = mako::leer(exe);
  const bool generador = mako::generadorInstalado(), lsfg = !mako::dllLossless().empty();
  const int modelo = p.existe ? (p.modelo == "lossless-scaling" ? 0 : 1) : (generador || !lsfg ? 1 : 0);
  static const int kFlujo[4] = {50, 75, 85, 100};
  int flujo = 0;
  for (int k = 1; k < 4; k++)
    if (std::abs((int)std::lround(p.flujo * 100.0) - kFlujo[k]) < std::abs((int)std::lround(p.flujo * 100.0) - kFlujo[flujo])) flujo = k;
  auto opciones = [&](const std::string &rot, std::vector<std::string> opc, int elegido, const char *que) {
    Hud::ItemPanel o;
    o.tipo = Hud::ItemPanel::Opciones, o.rotulo = rot, o.opciones = std::move(opc), o.elegido = elegido;
    pon(o, idDe(que));
  };
  opciones(idioma::tr("Generación"), {idioma::tr("Apagada"), idioma::tr("Encendida")}, p.existe && p.puesta ? 1 : 0, "puesta");
  opciones(idioma::tr("Modelo"), {"LSFG", "MAKO FG"}, modelo, "modelo");
  opciones(idioma::tr("Multiplicador"), {"x2", "x3", "x4"}, std::max(0, std::min(2, p.multiplicador - 2)), "multiplicador");
  opciones(idioma::tr("Flujo"), {"50 %", "75 %", "85 %", "100 %"}, flujo, "flujo");
  opciones(idioma::tr("Modo rendimiento"), {idioma::tr("No"), idioma::tr("Sí")}, p.rendimiento ? 1 : 0, "rendimiento");
}

/* (v1.18) una fila del perfil de MAKO de `exe`: la de la pestana FG o la de las Integraciones de un juego */
static bool aplicarPerfilMako(const std::string &exe, const std::string &que, int sub, std::string *aviso);

static void filasFg(Hud::State &st, std::vector<std::string> &ids) {
  auto pon = [&](Hud::ItemPanel it, const std::string &id) {
    st.items.push_back(std::move(it));
    ids.push_back(id);
  };
  Hud::ItemPanel sec;
  sec.tipo = Hud::ItemPanel::Seccion, sec.rotulo = "MAKO";
  pon(sec, "");
  if (!mako::instalado()) {
    Hud::ItemPanel n;
    n.tipo = Hud::ItemPanel::Nota;
    n.rotulo = idioma::tr("MAKO no está instalado: la generación de cuadros es su capa de Vulkan (mako-render).");
    pon(n, "");
  } else {
    /* (v1.18) su desinstalador (Rodolfo: «tambien añade desinstalador de mako»): dos clics, como el de OdysseyVK */
    Hud::ItemPanel b;
    b.tipo = Hud::ItemPanel::Boton, b.rotulo = idioma::tr("Desinstalar MAKO");
    b.nota = idioma::tr("La capa y su MAKO FG; pide tu clave para su paquete. Su configuración se queda.");
    b.valorTexto = idioma::tr(g_confirmarMako > (long)::time(nullptr) ? "¿Seguro? Pica otra vez" : "Desinstalar");
    b.valor = true, b.peligro = true;
    pon(b, "fg:desinstalar");
  }
  /* su generador propio: el de MAKO, el de OdysseyVK, bajando, o su boton */
  {
    const mako::Instalacion in = mako::leerInstalacion();
    const std::string sis = mako::hostSistema(), propio = mako::hostPropio();
    const long megas = (mako::bytesGenerador() + 999999) / 1000000;
    Hud::ItemPanel b;
    if (in.enMarcha) {
      b.tipo = Hud::ItemPanel::Nota;
      b.rotulo = idioma::trf("Bajando MAKO FG… {} de {} MB", {std::to_string(in.hecho / 1000000), std::to_string(megas)});
      pon(b, "");
    } else if (!sis.empty()) {
      b.tipo = Hud::ItemPanel::Nota;
      b.rotulo = idioma::tr("MAKO FG: el de MAKO, junto a su biblioteca");
      pon(b, "");
    } else if (!propio.empty()) {
      /* el que instalo OdysseyVK: se puede quitar solo el */
      b.tipo = Hud::ItemPanel::Boton, b.rotulo = "MAKO FG";
      b.nota = idioma::trf("MAKO FG {} instalado: los juegos con MAKO lo reciben con MAKO_FG_HOST (uso no comercial, CC BY-NC 4.0)",
                           {mako::kVersionGenerador});
      b.valorTexto = idioma::tr(g_confirmarMakoFg > (long)::time(nullptr) ? "¿Seguro? Pica otra vez" : "Quitar");
      b.valor = true, b.peligro = true;
      pon(b, "fg:quitar_fg");
    } else {
      b.tipo = Hud::ItemPanel::Boton, b.rotulo = "MAKO FG";
      b.nota = !in.error.empty() ? idioma::trf("No se pudo instalar MAKO FG: {}", {in.error})
                                 : idioma::trf("El modelo propio de MAKO ({} MB, no pide root; uso no comercial, CC BY-NC 4.0)", {std::to_string(megas)});
      b.valorTexto = idioma::tr(in.error.empty() ? "Instalar" : "Reintentar"), b.valor = true;
      pon(b, "fg:instalar");
    }
  }
  /* (v1.18) los videos: MAKO FG dentro del visor, aparte de los juegos (fgvideo.h) */
  {
    Hud::ItemPanel sv;
    sv.tipo = Hud::ItemPanel::Seccion, sv.rotulo = idioma::tr("VÍDEOS");
    pon(sv, "");
    const bool hay = mako::generadorInstalado();
    /* (v1.18) Rodolfo: «dejalo con multiplicadores, que se active con un toggle que se llame activar en videos» */
    Hud::ItemPanel sw;
    sw.tipo = Hud::ItemPanel::Interruptor, sw.rotulo = idioma::tr("Activar en vídeos");
    sw.valor = fgVideoPuesta() > 0;
    sw.ayuda = idioma::tr("Cuadros intermedios para los vídeos (no los juegos), hechos con MAKO FG dentro del visor. El vídeo va un poco por detrás: 21 ms a 24 fps con x2.");
    if (!hay) sw.apagado = true;
    pon(sw, "fg:video_activo");
    Hud::ItemPanel o;
    o.tipo = Hud::ItemPanel::Opciones, o.rotulo = idioma::tr("Multiplicador");
    o.opciones = {"x2", "x3", "x4"};
    o.elegido = fgVideoMultiplicador() - 2;
    if (!hay) o.apagado = true;
    pon(o, "fg:video");
    Hud::ItemPanel n;
    n.tipo = Hud::ItemPanel::Nota;
    n.rotulo = hay ? idioma::tr("Vale en un par de segundos con el 3D abierto sobre un vídeo; con un juego debajo no hace nada.")
                   : idioma::tr("Hace falta MAKO FG: instálalo arriba.");
    pon(n, "");
  }
  if (!mako::instalado()) return;
  /* el perfil del juego que corre */
  const JuegoFg j = juegoFg();
  Hud::ItemPanel sj;
  sj.tipo = Hud::ItemPanel::Seccion, sj.rotulo = idioma::tr("EL JUEGO");
  pon(sj, "");
  if (!j.exe.empty()) {   /* el nombre tal cual: las cabeceras cambian las mayusculas */
    Hud::ItemPanel ne;
    ne.tipo = Hud::ItemPanel::Nota, ne.rotulo = j.exe;
    pon(ne, "");
  }
  if (j.exe.empty()) {
    Hud::ItemPanel n;
    n.tipo = Hud::ItemPanel::Nota, n.rotulo = idioma::tr("Sin un juego debajo: la generación de cuadros va en el juego, no en el vídeo.");
    pon(n, "");
    return;
  }
  filasPerfilMako(st, ids, j.exe, "");
  Hud::ItemPanel n;
  n.tipo = Hud::ItemPanel::Nota;
  n.rotulo = !j.enJuego ? idioma::tr("MAKO no está cargado en este juego: pon ENABLE_MAKO=1 en Integraciones. Lo que elijas se guarda para cuando lo esté.")
                        : idioma::tr("Se guarda en el perfil de MAKO de este juego y vale en marcha; cambiar de modelo, al volver a abrir el juego.");
  pon(n, "");
}

/* lo que hace cada fila de la pestana FG: lo mismo que el menu del cuadrito (aplicarFg en vkview.cpp) */
static bool aplicarFg(const std::string &que, int sub, std::string *aviso) {
  /* (v1.18) desinstalar MAKO o quitar solo su MAKO FG: dos clics en 12 s */
  if (que == "desinstalar" || que == "quitar_fg") {
    long &hasta = que == "desinstalar" ? g_confirmarMako : g_confirmarMakoFg;
    const long ahora = (long)::time(nullptr);
    if (hasta <= ahora) {
      hasta = ahora + 12;
      if (aviso) *aviso = idioma::tr("¿seguro? pica otra vez en los próximos 12 segundos");
      return true;
    }
    hasta = 0;
    std::string err;
    if (!mako::lanzarOrden(que == "desinstalar" ? "desinstalar" : "quitar", &err)) {
      if (aviso) *aviso = err;
      return true;
    }
    const bool todo = que == "desinstalar";
    if (aviso) *aviso = idioma::tr(todo ? "desinstalando MAKO: se pide tu clave" : "quitando MAKO FG…");
    return true;
  }
  if (que == "instalar") {
    std::string err;
    const bool bien = mako::lanzarInstalacion(&err);
    if (aviso) *aviso = bien ? idioma::tr("instalando MAKO FG…") : idioma::trf("No se pudo instalar MAKO FG: {}", {err});
    return true;
  }
  /* (v1.18) los videos: el interruptor y el multiplicador (el visor los mira cada 2 s) */
  if (que == "video_activo") {
    std::string e;
    const bool poner = fgVideoPuesta() == 0;
    if (!ajuste::fijarEnServeConf("fg_video_activo", poner ? "si" : "no", &e)) {
      if (aviso) *aviso = idioma::trf("no se pudo guardar: {}", {e});
      return true;
    }
    if (aviso) *aviso = poner ? idioma::trf("generación en vídeos: x{}", {std::to_string(fgVideoMultiplicador())})
                              : std::string(idioma::tr("generación en vídeos apagada"));
    return true;
  }
  if (que == "video") {
    std::string e;
    const std::string val = std::to_string(std::max(2, std::min(4, sub + 2)));
    if (!ajuste::fijarEnServeConf("fg_video", val, &e)) {
      if (aviso) *aviso = idioma::trf("no se pudo guardar: {}", {e});
      return true;
    }
    if (aviso) *aviso = idioma::trf("generación en vídeos: x{}", {val});
    return true;
  }
  const JuegoFg j = juegoFg();
  return aplicarPerfilMako(j.exe, que, sub, aviso);
}

static bool aplicarPerfilMako(const std::string &exe, const std::string &que, int sub, std::string *aviso) {
  if (exe.empty() || !mako::instalado()) return false;
  static const int kFlujo[4] = {50, 75, 85, 100};
  std::map<std::string, std::string> c;
  if (que == "puesta") c["frame_generation_enabled"] = sub == 1 ? "true" : "false";
  else if (que == "modelo") {
    if (sub == 0 ? mako::dllLossless().empty() : !mako::generadorInstalado()) {
      if (aviso) *aviso = idioma::tr(sub == 0 ? "Lossless Scaling no está en Steam: elige MAKO FG." : "MAKO FG no está instalado: instálalo con el botón (no pide root); mientras, LSFG ya genera.");
      return true;
    }
    c["frame_generation_model"] = sub == 0 ? "'lossless-scaling'" : "'mako-fg'";
  } else if (que == "multiplicador") c["multiplier"] = std::to_string(std::max(2, std::min(4, sub + 2)));
  else if (que == "flujo") {
    char b[16];
    std::snprintf(b, sizeof b, "%.2f", kFlujo[std::max(0, std::min(3, sub))] / 100.0);
    c["flow_scale"] = b;
  } else if (que == "rendimiento") c["performance_mode"] = sub == 1 ? "true" : "false";
  else return false;
  /* un perfil nuevo lleva tambien lo demas, como se ve en la pestana */
  const mako::Perfil p = mako::leer(exe);
  if (!p.existe) {
    char b[16];
    std::snprintf(b, sizeof b, "%.2f", p.flujo);
    c.emplace("frame_generation_enabled", "false");
    c.emplace("multiplier", std::to_string(p.multiplicador));
    c.emplace("flow_scale", b);
    c.emplace("performance_mode", p.rendimiento ? "true" : "false");
    c.emplace("frame_generation_model", mako::generadorInstalado() || mako::dllLossless().empty() ? "'mako-fg'" : "'lossless-scaling'");
  }
  std::string err;
  if (!mako::fijar(exe, c, &err)) {
    if (aviso) *aviso = idioma::trf("MAKO no lo aceptó: {}", {err});
    return true;
  }
  if (aviso) *aviso = que == "modelo" ? idioma::tr("guardado; vale al volver a abrir el juego") : idioma::trf("guardado en el perfil de MAKO de {}", {exe});
  return true;
}

/* ---- (v1.18) la pestana de los efectos de ReShade (efectos.h) -----------------------------------------------------
   Rodolfo: «the shaders must have a toggle pill, each of them a scrollbar and their respective setting sliders, do the
   settings in your own way» y «beside games, call it effects». La pestana 5, que se ve junto a Juegos, abre los efectos
   de todos los juegos; el icono de efectos de cada caratula (Juegos), los de ese juego. La pagina: el interruptor de todos, y cada efecto con su
   cabecera plegable, su pastilla y sus deslizadores (los de su .fx, con los valores de fabrica de los favoritos de
   Rodolfo); debajo, añadir uno de la carpeta. Lo que se toca se escribe al momento y el visor lo recoge en medio segundo */
namespace {
std::string g_efectosDe;               /* los efectos que ensena la pestana: vacio o "*" = los de todos los juegos, o un appid */
bool g_efectosDesdeJuegos = false;     /* se abrio desde una caratula: Volver lleva a Juegos */
std::string g_juegoDelPanel;           /* el juego sobre el que se abrio el panel (un boton a sus efectos) */
bool g_fuentesSolas = false;           /* (v1.18) el panel de verdad mira las fuentes solo (la prueba del HUD, no: sin red) */

std::string numeroEfecto(float v, const efectos::Control &c) {
  if (c.tipo == efectos::Control::Entero || c.tipo == efectos::Control::Lista) return std::to_string((long)std::lround(v));
  int dec = 3;
  if (c.paso > 0.0f) {
    dec = 0;
    for (float p = c.paso; p < 0.999f && dec < 4; p *= 10.0f) dec++;
  }
  char b[48];
  std::snprintf(b, sizeof b, "%.*f", dec, (double)v);
  std::string s = b;
  if (s.find('.') != std::string::npos) {
    while (!s.empty() && s.back() == '0') s.pop_back();
    if (!s.empty() && s.back() == '.') s.pop_back();
  }
  if (s == "-0") s = "0";
  return s;
}
/* el valor de fabrica de un control: el de los favoritos de Rodolfo si lo dicen, si no el del .fx */
float fabricaDe(const std::string &efecto, const efectos::Control &c, int comp) {
  static const efectos::Ajustes fab = efectos::deFabrica();
  for (const efectos::Efecto &e : fab.lista)
    if (e.nombre == efecto) return efectos::valorDe(e, c, comp);
  return c.defecto[std::clamp(comp, 0, 3)];
}
std::string nombreDeJuego(const std::vector<Juego> &juegos, const std::string &app) {
  for (const Juego &j : juegos)
    if (j.id == app) return j.nombre;
  return app;
}
}  // namespace

/* (v1.18) ShaderBeam (haz.h). Rodolfo, señalando https://github.com/mausimus/ShaderBeam: «create this one too». Va en la
   pestana Efectos, con el ambito de los efectos (todos los juegos o uno), pero no es un efecto de ReShade: corre en la trama a
   cada refresco del panel. Su pastilla y, puesta, sus ajustes (los de ShaderBeam que hacen falta aqui) */
static std::string numeroHaz(float v, int decimales) {
  char t[32];
  std::snprintf(t, sizeof t, "%.*f", decimales, (double)v);
  return t;
}
static std::vector<std::string> nombresBarrido() {
  return {idioma::tr("Ninguno (todo a la vez)"), idioma::tr("De arriba abajo"), idioma::tr("De abajo arriba"),
          idioma::tr("De izquierda a derecha"), idioma::tr("De derecha a izquierda")};
}
template <class Pon> static void filasHaz(const haz::Ajustes &h, Pon &&pon) {
  Hud::ItemPanel s;
  s.tipo = Hud::ItemPanel::Seccion, s.rotulo = idioma::tr("SHADERBEAM · NITIDEZ EN MOVIMIENTO"), s.talCual = true;
  pon(s, "");
  Hud::ItemPanel p;
  p.tipo = Hud::ItemPanel::Interruptor, p.rotulo = "ShaderBeam", p.valor = h.puesto, p.pildora = true;
  p.nota = idioma::tr("Mucha menos estela al moverse, a cambio de brillo. Parpadea: cuidado si eres fotosensible");
  p.ayuda = idioma::tr("Blur Busters CRT Beam Simulator (Mark Rejhon y Timothy Lottes), como en ShaderBeam (mausimus). Va bien con un juego fijo a 60 cuadros; con la generación de cuadros o a más de 100 no hace falta.");
  pon(p, "haz:puesto");
  if (!h.puesto) return;
  Hud::ItemPanel m;
  m.tipo = Hud::ItemPanel::Opciones, m.rotulo = idioma::tr("Modo");
  m.opciones = {idioma::tr("Haz del CRT"), idioma::tr("BFI simple")};
  m.elegido = h.modo == haz::BfiSimple ? 1 : 0;
  m.ayuda = idioma::tr("BFI simple: un refresco con imagen y los demás en negro. Más oscuro y parpadea más; el haz del CRT reparte la luz.");
  pon(m, "haz:modo");
  Hud::ItemPanel z;
  z.tipo = Hud::ItemPanel::Deslizador, z.rotulo = idioma::tr("Hz del CRT");
  z.frac = (h.hz - haz::kHzMin) / (haz::kHzMax - haz::kHzMin);
  z.fracDefecto = (60.0f - haz::kHzMin) / (haz::kHzMax - haz::kHzMin);
  z.valorTexto = numeroHaz(h.hz, 0) + " Hz";
  z.ayuda = idioma::tr("Los cuadros por segundo del juego: pon el juego fijo a esos cuadros. A 165 Hz, 60 son 2,75 refrescos por cuadro y 55 son 3.");
  pon(z, "haz:hz");
  if (h.modo == haz::BfiSimple) return;
  Hud::ItemPanel g;
  g.tipo = Hud::ItemPanel::Deslizador, g.rotulo = idioma::tr("Brillo contra estela");
  g.frac = (h.ganancia - haz::kGananciaMin) / (haz::kGananciaMax - haz::kGananciaMin);
  g.fracDefecto = (0.5f - haz::kGananciaMin) / (haz::kGananciaMax - haz::kGananciaMin);
  g.valorTexto = numeroHaz(h.ganancia, 2);
  g.ayuda = idioma::tr("1 es el brillo de siempre, pero los colores claros no pierden estela; 0,5 es la mitad de brillo y menos estela en todos.");
  pon(g, "haz:ganancia");
  Hud::ItemPanel a;
  a.tipo = Hud::ItemPanel::Deslizador, a.rotulo = idioma::tr("Gamma del panel");
  a.frac = (h.gamma - haz::kGammaMin) / (haz::kGammaMax - haz::kGammaMin);
  a.fracDefecto = (2.2f - haz::kGammaMin) / (haz::kGammaMax - haz::kGammaMin);
  a.valorTexto = numeroHaz(h.gamma, 2);
  a.ayuda = idioma::tr("Si se ven franjas horizontales que bajan despacio, muévela hasta que se vayan.");
  pon(a, "haz:gamma");
  Hud::ItemPanel b;
  b.tipo = Hud::ItemPanel::Desplegable, b.rotulo = idioma::tr("Barrido");
  const std::vector<std::string> dirs = nombresBarrido();
  b.elegido = std::clamp(h.barrido, 0, 4);
  b.valorTexto = idioma::trf("Barrido: {}", {dirs[(size_t)b.elegido]});   /* el desplegable no lleva rotulo aparte */
  b.ayuda = idioma::tr("Por donde pasa el haz. Ninguno quita las franjas, pero se ve peor.");
  std::string idl = "lista:haz-barrido";
  for (size_t o = 0; o < dirs.size(); o++) {
    b.opciones.push_back(dirs[o]), b.ayudas.push_back(""), b.estadosOpc.push_back(0);
    idl += '\x1f' + std::string("haz:barrido:") + std::to_string(o);
  }
  pon(b, idl);
}

static bool aplicarHaz(const std::string &que, int sub, float frac, std::string *aviso) {
  const std::string app = g_efectosDe == "*" ? std::string() : g_efectosDe;
  efectos::Ajustes a = efectos::cargar(app);
  haz::Ajustes &h = a.haz;
  std::string dicho;
  const float f = std::clamp(frac, 0.0f, 1.0f);
  if (que == "puesto") {
    h.puesto = !h.puesto;
    dicho = idioma::tr(h.puesto ? "ShaderBeam encendido" : "ShaderBeam apagado");
  } else if (que == "modo") {
    if (sub < 0 || sub > 1) return false;
    h.modo = sub == 1 ? haz::BfiSimple : haz::Crt;
    dicho = idioma::tr(sub == 1 ? "ShaderBeam: BFI simple" : "ShaderBeam: haz del CRT");
  } else if (que == "hz") {
    h.hz = std::round(haz::kHzMin + f * (haz::kHzMax - haz::kHzMin));
    dicho = std::string(idioma::tr("Hz del CRT")) + " " + numeroHaz(h.hz, 0);
  } else if (que == "ganancia") {
    h.ganancia = std::round((haz::kGananciaMin + f * (haz::kGananciaMax - haz::kGananciaMin)) * 100.0f) / 100.0f;
    dicho = std::string(idioma::tr("Brillo contra estela")) + " " + numeroHaz(h.ganancia, 2);
  } else if (que == "gamma") {
    h.gamma = std::round((haz::kGammaMin + f * (haz::kGammaMax - haz::kGammaMin)) * 100.0f) / 100.0f;
    dicho = std::string(idioma::tr("Gamma del panel")) + " " + numeroHaz(h.gamma, 2);
  } else if (que.rfind("barrido:", 0) == 0) {
    const int o = std::atoi(que.substr(8).c_str());
    if (o < 0 || o > 4) return false;
    h.barrido = o;
    dicho = idioma::trf("Barrido: {}", {nombresBarrido()[(size_t)o]});
  } else {
    return false;
  }
  std::string err;
  if (!efectos::guardar(app, a, &err)) {
    if (aviso) *aviso = idioma::trf("no se pudo guardar: {}", {err});
    return false;
  }
  if (aviso) *aviso = dicho;
  return true;
}

static void filasEfectos(Hud::State &st, std::vector<std::string> &ids, const std::vector<Juego> &juegos) {
  /* (v1.18) en dos mitades (Rodolfo: «divide the effect space at the bottom half into two parts, in half», «el menu normal
     se queda asi mitad y mitad»): a la izquierda los efectos de la lista con sus ajustes; a la derecha, añadir y las
     fuentes publicas. Con la vista previa palomeada, todo en la columna de la izquierda (sin las notas ni las fuentes) */
  const bool previa = st.vistaEfectos && st.marco;
  int lado = 0;
  auto pon = [&](Hud::ItemPanel it, const std::string &id) {
    if (st.marco && !previa) it.lado = lado;
    st.items.push_back(std::move(it));
    ids.push_back(id);
  };
  /* la casilla de la vista previa: en medio, encima de las dos mitades, como la de la vista de depuracion (Rodolfo: «pon la
     casilla de preview para palomearla igual que pusiste la de depuracion en la otra seccion»); palomeada, arriba de la
     columna. Solo con el panel sobre el 3D de un juego: el visor es el que pinta el juego */
  if (st.marco) {
    Hud::ItemPanel d;
    d.tipo = Hud::ItemPanel::Interruptor;
    d.rotulo = idioma::tr("Vista previa");
    d.valor = previa;
    d.centrado = !previa;
    d.apagado = !st.vistaEnVivo && !previa;
    if (previa) d.nota = idioma::tr("El juego con sus efectos, a pantalla completa");
    d.ayuda = idioma::tr(st.vistaEnVivo ? "El juego a pantalla completa con sus efectos, y la lista a un lado para ajustarlos."
                                        : "Hace falta un juego en 3D abierto: enseña el juego con sus efectos.");
    pon(d, "vista_efectos");
  }
  const std::string app = g_efectosDe == "*" ? std::string() : g_efectosDe;   /* vacio: los de todos */
  const std::string nombre = app.empty() ? std::string() : nombreDeJuego(juegos, app);
  const efectos::Ajustes a = efectos::cargar(app);
  st.efectosPuestos = a.puestos;   /* (v1.18) la vista previa dice si estan apagados */
  const auto disp = efectos::disponibles();
  const std::string carpeta = efectos::carpeta();
  Hud::ItemPanel s;
  s.tipo = Hud::ItemPanel::Seccion;
  s.rotulo = app.empty() ? std::string(idioma::tr("EFECTOS · TODOS LOS JUEGOS")) : idioma::trf("EFECTOS · {}", {nombre});
  pon(s, "");
  if (!app.empty()) {   /* los de un juego: su nombre y volver a los de todos (o a Juegos, si se vino de su caratula) */
    Hud::ItemPanel b;
    b.tipo = Hud::ItemPanel::Boton, b.rotulo = nombre;
    b.nota = idioma::tr(a.propio ? "Este juego tiene los suyos" : "Como todos los juegos: lo que cambies aquí será solo de este");
    b.valorTexto = idioma::tr("Atrás"), b.valor = true;
    pon(b, "efx:volver");
  } else {
    Hud::ItemPanel n0;
    n0.tipo = Hud::ItemPanel::Nota, n0.rotulo = idioma::tr("Los de cada juego que no tenga los suyos");
    pon(n0, "");
    /* sobre un juego: un boton a los suyos */
    if (!g_juegoDelPanel.empty()) {
      const efectos::Ajustes suyos = efectos::cargar(g_juegoDelPanel);
      Hud::ItemPanel j;
      j.tipo = Hud::ItemPanel::Boton, j.rotulo = idioma::trf("Efectos de {}", {nombreDeJuego(juegos, g_juegoDelPanel)});
      j.nota = idioma::tr(suyos.propio ? "Este juego tiene los suyos" : "Como todos los juegos");
      j.valorTexto = idioma::tr("Efectos"), j.valor = true;
      pon(j, "efx:abrir:" + g_juegoDelPanel);
    }
  }
  if (!app.empty() && a.propio) {
    Hud::ItemPanel t;
    t.tipo = Hud::ItemPanel::Boton, t.rotulo = idioma::tr("Volver a los de todos los juegos");
    t.nota = idioma::tr("Se borran los de este juego");
    t.valorTexto = idioma::tr("Como todos");
    pon(t, "efx:como_todos");
  }
  if (carpeta.empty()) {
    Hud::ItemPanel in;
    in.tipo = Hud::ItemPanel::Info, in.rotulo = idioma::tr("No encontré la carpeta de los efectos");
    in.nota = idioma::tr("Van en ~/.config/odysseyvk/reshade (Shaders/ y Textures/); otra, con efectos_carpeta en serve.conf.");
    pon(in, "");
  }
  lado = 1;
  Hud::ItemPanel m;
  m.tipo = Hud::ItemPanel::Interruptor, m.rotulo = idioma::tr("Efectos"), m.valor = a.puestos;
  m.nota = idioma::tr("Corren en el visor 3D sobre cada cuadro del juego, antes del 3D, con su profundidad");
  m.ayuda = carpeta;
  m.propio = !app.empty() && a.propio;
  m.pildora = true;
  m.sitioAspa = true;   /* (v1.18) alineado con los de los efectos, que llevan su aspita */
  pon(m, "efx:puestos");
  const std::set<std::string> &abiertas = seccionesAbiertas();
  for (size_t i = 0; i < a.lista.size(); i++) {
    const efectos::Efecto &e = a.lista[i];
    const auto it = disp.find(e.nombre);
    efectos::Descripcion d;
    if (it != disp.end()) d = efectos::describir(it->second);
    const bool abierta = abiertas.count("efx:" + e.nombre) > 0;
    /* (v1.18) un renglon por efecto: la flechita y su nombre a la izquierda (pliegan), su pastilla a la derecha */
    Hud::ItemPanel p;
    p.tipo = Hud::ItemPanel::Interruptor, p.rotulo = e.nombre, p.valor = e.activo, p.pildora = true;
    p.plegable = true, p.desplegada = abierta;
    /* (v1.18) el agarre para cambiarlo de puesto y el aspita para quitarlo (en vez de «Subir» y «Quitar» al desplegarlo) */
    p.arrastrable = true, p.borrable = true, p.bloque = (int)i;
    if (it == disp.end()) {
      p.apagado = true, p.pastilla = idioma::tr("No está"), p.nota = idioma::tr("No está en la carpeta de ReShade");
    } else if (!d.ok) {
      p.apagado = true, p.pastilla = idioma::tr("No compila"), p.nota = d.error.substr(0, 160);
    } else if (d.computo) {
      p.apagado = true, p.pastilla = idioma::tr("No corre aquí"), p.nota = idioma::tr("Lleva pases de cómputo");
    } else {
      p.nota = it->second.size() > carpeta.size() + 1 && it->second.rfind(carpeta, 0) == 0 ? it->second.substr(carpeta.size() + 1) : it->second;
      if (d.usaProfundidad) p.nota += " · " + std::string(idioma::tr("usa la profundidad del 3D"));
    }
    pon(p, "efx:on:" + e.nombre);
    if (!abierta || !d.ok) continue;
    for (const efectos::Control &c : d.controles)
      for (int k = 0; k < c.componentes && k < 4; k++) {
        const std::string clave = efectos::claveDe(c, k);
        const float val = efectos::valorDe(e, c, k);
        std::string rot = c.rotulo;
        if (c.componentes > 1) rot += c.color ? std::string(" ") + "RGBA"[k] : " [" + std::to_string(k) + "]";
        const std::string id = "efx:v:" + e.nombre + ":" + clave;
        Hud::ItemPanel f;
        f.ayuda = c.ayuda;
        f.bloque = (int)i;
        if (c.tipo == efectos::Control::Bool) {
          f.tipo = Hud::ItemPanel::Interruptor, f.rotulo = rot, f.valor = val > 0.5f;
          pon(f, id);
        } else if (c.tipo == efectos::Control::Lista && c.opciones.size() <= 4) {
          f.tipo = Hud::ItemPanel::Opciones, f.rotulo = rot, f.opciones = c.opciones, f.elegido = (int)std::lround(val);
          pon(f, id);
        } else if (c.tipo == efectos::Control::Lista) {
          f.tipo = Hud::ItemPanel::Desplegable, f.rotulo = rot, f.elegido = (int)std::lround(val);
          std::string idl = "lista:efx-" + e.nombre + "-" + clave;
          for (size_t o = 0; o < c.opciones.size(); o++) {
            f.opciones.push_back(c.opciones[o]), f.ayudas.push_back(""), f.estadosOpc.push_back(0);
            idl += '\x1f' + std::string("efx:lista:") + std::to_string(o) + ":" + e.nombre + ":" + clave;
          }
          f.valorTexto = f.elegido >= 0 && (size_t)f.elegido < c.opciones.size() ? c.opciones[(size_t)f.elegido] : std::string();
          pon(f, idl);
        } else {
          const float rango = c.maximo - c.minimo;
          f.tipo = Hud::ItemPanel::Deslizador, f.rotulo = rot;
          f.frac = std::clamp((val - c.minimo) / rango, 0.0f, 1.0f);
          f.valorTexto = numeroEfecto(val, c);
          f.fracDefecto = std::clamp((fabricaDe(e.nombre, c, k) - c.minimo) / rango, 0.0f, 1.0f);
          pon(f, id);
        }
      }
    if (d.controles.empty()) {
      Hud::ItemPanel n;
      n.tipo = Hud::ItemPanel::Nota, n.rotulo = idioma::tr("No tiene ajustes");
      n.bloque = (int)i;
      pon(n, "");
    }
  }
  /* añadir uno de la carpeta */
  lado = 2;
  Hud::ItemPanel sa;
  sa.tipo = Hud::ItemPanel::Seccion, sa.rotulo = idioma::tr("AÑADIR UN EFECTO");
  pon(sa, "");
  Hud::ItemPanel dsp;
  dsp.tipo = Hud::ItemPanel::Desplegable, dsp.elegido = -1, dsp.valorTexto = idioma::tr("Elige uno…");
  /* (v1.18) en dos grupos, los que usan la profundidad y los que no, en la tarjeta entera (ver ItemPanel::grupos) */
  dsp.grupos = {idioma::tr("Usan la profundidad"), idioma::tr("Sin profundidad")};
  std::string idd = "lista:efx-anadir";
  for (const auto &[n, r] : disp) {
    bool ya = false;
    for (const efectos::Efecto &e : a.lista) ya = ya || e.nombre == n;
    if (ya) continue;
    dsp.opciones.push_back(n), dsp.ayudas.push_back(""), dsp.estadosOpc.push_back(0);
    dsp.grupoOpc.push_back(efectos::usaProfundidad(r) ? 0 : 1);
    idd += '\x1f' + std::string("efx:anadir:") + n;
  }
  pon(dsp, idd);
  if (previa) return;   /* en la columna, solo lo que se prueba con el juego a la vista */
  filasHaz(a.haz, pon);   /* (v1.18) ShaderBeam, en la mitad de la derecha */
  Hud::ItemPanel n;
  n.tipo = Hud::ItemPanel::Nota;
  n.rotulo = carpeta.empty() ? std::string() : idioma::trf("{} efectos en {}", {std::to_string(disp.size()), carpeta});
  if (!n.rotulo.empty()) pon(n, "");
  if (app.empty()) {
    Hud::ItemPanel n2;
    n2.tipo = Hud::ItemPanel::Nota;
    n2.rotulo = idioma::tr("Los efectos (.fx) de ReShade corren en el visor 3D sobre cada cuadro del juego, con la profundidad del 3D. Cada juego puede tener los suyos: el icono de efectos de su carátula, en Juegos.");
    pon(n2, "");
  }
  /* (v1.18) las fuentes publicas (efectos.h): en la de todos los juegos, con su boton de bajar solo lo que falta. Se mira
     sola al abrir si la ultima vez fue hace mas de 12 horas, como lo de la version */
  if (!app.empty() || carpeta.empty()) return;
  const bool mirando = efectos::comprobando();
  const long hora = efectos::horaComprobacion();
  static bool pedida = false;
  if (g_fuentesSolas && !pedida && !mirando && (long)std::time(nullptr) - hora > 12 * 3600) {
    pedida = true;
    efectos::lanzarOrden({"fuentes"});
  }
  Hud::ItemPanel sf;
  sf.tipo = Hud::ItemPanel::Seccion, sf.rotulo = idioma::tr("FUENTES PÚBLICAS");
  pon(sf, "");
  Hud::ItemPanel bm;
  bm.tipo = Hud::ItemPanel::Boton, bm.rotulo = idioma::tr("Buscar cambios en GitHub");
  if (mirando) {
    bm.nota = idioma::tr("Mirando lo que falta de cada una…");
  } else if (hora > 0) {
    char cuando[32];
    const time_t t = (time_t)hora;
    std::strftime(cuando, sizeof cuando, "%d/%m %H:%M", std::localtime(&t));
    bm.nota = idioma::trf("Mirado el {}: solo se baja lo que no tienes, comparado por su contenido", {cuando});
  } else {
    bm.nota = idioma::tr("Solo se baja lo que no tienes, comparado por su contenido");
  }
  bm.valorTexto = idioma::tr("Buscar"), bm.valor = !mirando;
  pon(bm, "efx:fuentes");
  const auto estados = efectos::leerFuentes();
  const efectos::Bajada baj = efectos::leerBajada();
  auto tamano = [](long long b) {
    char t[32];
    if (b < 1024 * 1024) std::snprintf(t, sizeof t, "%lld KB", std::max(1LL, (b + 1023) / 1024));
    else std::snprintf(t, sizeof t, "%.1f MB", (double)b / (1024.0 * 1024.0));
    return std::string(t);
  };
  for (const efectos::Fuente &f : efectos::fuentesPublicas()) {
    Hud::ItemPanel r;
    r.tipo = Hud::ItemPanel::Boton, r.rotulo = idioma::tr(f.nombre);
    r.nota = std::string(f.repo) + " · " + idioma::tr(f.nota);
    const auto it = estados.find(f.id);
    const bool bajando = baj.enMarcha && baj.id == f.id;
    if (bajando) {
      r.valorTexto = baj.total > 0 ? idioma::trf("Bajando {} de {}", {std::to_string(baj.hecho), std::to_string(baj.total)})
                                   : std::string(idioma::tr("Mirando…"));
      r.valor = false;
    } else if (it == estados.end()) {
      r.valorTexto = idioma::tr("Descargar"), r.valor = !mirando && !baj.enMarcha;
    } else if (it->second.error == "no-encontrada") {
      r.valorTexto = idioma::tr("No está en GitHub"), r.valor = false;
    } else if (!it->second.error.empty()) {
      r.nota += " · " + std::string(it->second.error == "limite" ? idioma::tr("GitHub no respondió por ahora") : it->second.error);
      r.valorTexto = idioma::tr("Reintentar"), r.valor = !mirando && !baj.enMarcha;
    } else if (it->second.nuevos == 0) {
      r.valorTexto = idioma::tr("Ya la tienes"), r.valor = false;   /* en gris: nada que bajar (nunca se pisa lo que hay) */
    } else {
      const int n = it->second.nuevos;
      r.valorTexto = idioma::trf("Descargar · {} · {}", {n == 1 ? std::string(idioma::tr("1 archivo")) : idioma::trf("{} archivos", {std::to_string(n)}),
                                                        tamano(it->second.bytes)});
      r.valor = !mirando && !baj.enMarcha;
      r.nota += " · " + idioma::trf("{} nuevos", {std::to_string(it->second.nuevos)});
    }
    if (it != estados.end() && it->second.conflictos > 0)
      r.nota += " · " + idioma::trf("{} ya los tienes con ese nombre: se quedan los tuyos", {std::to_string(it->second.conflictos)});
    if (!bajando && baj.id == f.id && !baj.error.empty())
      r.nota += " · " + idioma::trf("no se pudo: {}", {baj.error == "cortada" ? std::string(idioma::tr("se cortó a medias")) : baj.error});
    pon(r, std::string("efx:fuente:") + f.id);
  }
}

static bool aplicarEfectos(const std::string &que, int sub, float frac, std::string *aviso) {
  if (que.rfind("abrir:", 0) == 0) {
    g_efectosDe = que.substr(6), g_efectosDesdeJuegos = false;
    g_pestanaPedida = 5;
    return true;
  }
  /* (v1.18) las fuentes publicas: buscar cambios, o bajar lo que falta de una (o volver a mirarla si dio error) */
  if (que == "fuentes" || que.rfind("fuente:", 0) == 0) {
    if (efectos::comprobando() || efectos::leerBajada().enMarcha) {
      if (aviso) *aviso = idioma::tr("ya hay una en marcha: espera a que acabe");
      return false;
    }
    std::string err;
    if (que == "fuentes") {
      if (!efectos::lanzarOrden({"fuentes"}, &err)) return false;
      if (aviso) *aviso = idioma::tr("buscando cambios en GitHub…");
      return true;
    }
    const std::string id = que.substr(7);
    const auto est = efectos::leerFuentes();
    const auto it = est.find(id);
    const bool remirar = it != est.end() && !it->second.error.empty();
    if (!efectos::lanzarOrden(remirar ? std::vector<std::string>{"fuentes", id} : std::vector<std::string>{"bajar", id}, &err)) {
      if (aviso) *aviso = idioma::trf("no se pudo: {}", {err});
      return false;
    }
    if (aviso) *aviso = idioma::tr(remirar ? "mirándola otra vez…" : "bajando solo lo que falta…");
    return true;
  }
  if (que == "volver") {   /* a los de todos, o a Juegos si se vino de una caratula */
    const bool aJuegos = g_efectosDesdeJuegos;
    g_efectosDe.clear(), g_efectosDesdeJuegos = false;
    g_pestanaPedida = aJuegos ? 1 : 5;
    return true;
  }
  const std::string app = g_efectosDe == "*" ? std::string() : g_efectosDe;
  if (que == "como_todos") {
    efectos::quitarPropios(app);
    if (aviso) *aviso = idioma::tr("este juego vuelve a los efectos de todos los juegos");
    return true;
  }
  efectos::Ajustes a = efectos::cargar(app);
  std::string dicho;
  auto quitar = [&](const std::string &n) {
    a.lista.erase(std::remove_if(a.lista.begin(), a.lista.end(), [&](const efectos::Efecto &e) { return e.nombre == n; }), a.lista.end());
  };
  if (que == "puestos") {
    a.puestos = !a.puestos;
    dicho = idioma::tr(a.puestos ? "efectos encendidos" : "efectos apagados");
  } else if (que.rfind("on:", 0) == 0 && sub == 1) {
    /* la flechita o el nombre: se pliega o se despliega (no cambia los ajustes) */
    std::set<std::string> &ab = seccionesAbiertas();
    const std::string clave = "efx:" + que.substr(3);
    if (!ab.erase(clave)) ab.insert(clave);
    if (FILE *f = std::fopen(rutaEstadoOdyssey("panel-secciones").c_str(), "we")) {
      for (const std::string &c : ab) std::fprintf(f, "%s\n", c.c_str());
      std::fclose(f);
    }
    return true;
  } else if (que.rfind("on:", 0) == 0 && sub >= 200) {
    /* (v1.18) soltado por su agarre: a ese puesto de la lista (contado sin el, como lo pinta el panel) */
    const std::string n = que.substr(3);
    const auto it = std::find_if(a.lista.begin(), a.lista.end(), [&](const efectos::Efecto &e) { return e.nombre == n; });
    if (it == a.lista.end()) return false;
    const size_t desde = (size_t)(it - a.lista.begin());
    const size_t hasta = std::min((size_t)(sub - 200), a.lista.size() - 1);
    if (desde == hasta) return false;
    efectos::Efecto e = std::move(*it);
    a.lista.erase(it);
    a.lista.insert(a.lista.begin() + (ptrdiff_t)hasta, std::move(e));
    dicho = idioma::trf("{} pasa al puesto {}", {n, std::to_string(hasta + 1)});
  } else if (que.rfind("on:", 0) == 0 && sub == 3) {
    /* (v1.18) su aspita: fuera de la lista */
    if (!a.buscar(que.substr(3))) return false;
    quitar(que.substr(3));
    dicho = idioma::trf("{} quitado de la lista", {que.substr(3)});
  } else if (que.rfind("on:", 0) == 0 && sub == 2) {
    return false;   /* el agarre sin arrastrar: nada */
  } else if (que.rfind("on:", 0) == 0) {
    efectos::Efecto *e = a.buscar(que.substr(3));
    if (!e) return false;
    e->activo = !e->activo;
    dicho = idioma::trf(e->activo ? "{} encendido" : "{} apagado", {e->nombre});
  } else if (que.rfind("quitar:", 0) == 0) {
    quitar(que.substr(7));
    dicho = idioma::trf("{} quitado de la lista", {que.substr(7)});
  } else if (que.rfind("subir:", 0) == 0) {
    for (size_t i = 1; i < a.lista.size(); i++)
      if (a.lista[i].nombre == que.substr(6)) {
        std::swap(a.lista[i], a.lista[i - 1]);
        break;
      }
    dicho = idioma::trf("{} sube en la lista", {que.substr(6)});
  } else if (que.rfind("anadir:", 0) == 0) {
    const std::string n = que.substr(7);
    if (a.buscar(n)) return false;
    a.lista.push_back(efectos::Efecto{n, true, {}, {}});
    /* su cabecera, abierta: se ven sus deslizadores */
    seccionesAbiertas().insert("efx:" + n);
    if (FILE *f = std::fopen(rutaEstadoOdyssey("panel-secciones").c_str(), "we")) {
      for (const std::string &c : seccionesAbiertas()) std::fprintf(f, "%s\n", c.c_str());
      std::fclose(f);
    }
    dicho = idioma::trf("{} añadido", {n});
  } else if (que.rfind("v:", 0) == 0 || que.rfind("lista:", 0) == 0) {
    /* «v:EFECTO:CLAVE» (deslizador, interruptor u opciones) o «lista:K:EFECTO:CLAVE» (desplegable) */
    std::string resto = que.substr(que[0] == 'v' ? 2 : 6);
    long elegidaLista = -1;
    if (que[0] == 'l') {
      const size_t dp = resto.find(':');
      if (dp == std::string::npos) return false;
      elegidaLista = std::atol(resto.substr(0, dp).c_str());
      resto = resto.substr(dp + 1);
    }
    const size_t dp = resto.find(':');
    if (dp == std::string::npos) return false;
    const std::string nom = resto.substr(0, dp), clave = resto.substr(dp + 1);
    efectos::Efecto *e = a.buscar(nom);
    const auto disp = efectos::disponibles();
    const auto it = disp.find(nom);
    if (!e || it == disp.end()) return false;
    const efectos::Descripcion d = efectos::describir(it->second);
    for (const efectos::Control &c : d.controles)
      for (int k = 0; k < c.componentes && k < 4; k++) {
        if (efectos::claveDe(c, k) != clave) continue;
        std::string texto;
        if (c.tipo == efectos::Control::Bool) {
          const bool ahora = efectos::valorDe(*e, c, k) <= 0.5f;
          texto = ahora ? "true" : "false";
          dicho = c.rotulo + ": " + idioma::tr(ahora ? "Encendido" : "Apagado");
        } else if (c.tipo == efectos::Control::Lista) {
          const long o = elegidaLista >= 0 ? elegidaLista : sub;
          if (o < 0 || (size_t)o >= c.opciones.size()) return false;
          texto = std::to_string(o);
          dicho = c.rotulo + ": " + c.opciones[(size_t)o];
        } else {
          float v = c.minimo + std::clamp(frac, 0.0f, 1.0f) * (c.maximo - c.minimo);
          if (c.paso > 0.0f) v = c.minimo + std::round((v - c.minimo) / c.paso) * c.paso;
          if (c.tipo == efectos::Control::Entero) v = std::round(v);
          texto = numeroEfecto(v, c);
          dicho = c.rotulo + " " + texto;
        }
        e->fijar(clave, texto);
      }
    if (dicho.empty()) return false;
  } else {
    return false;
  }
  std::string err;
  if (!efectos::guardar(app, a, &err)) {
    if (aviso) *aviso = idioma::trf("no se pudo guardar: {}", {err});
    return false;
  }
  if (aviso) *aviso = dicho;
  return true;
}

void juegoDelPanel(const std::string &appid) { g_juegoDelPanel = appid; }
void fuentesSolas(bool si) { g_fuentesSolas = si; }
void efectosDe(const std::string &que) { g_efectosDe = que; }

static void llenarFilas(Hud::State &st, const Valores &v, const std::vector<Juego> &juegos, std::vector<std::string> &ids) {
  st.pieParametro.clear(), st.pieInterruptor = false;   /* (v1.14) el parametro de Steam del pie: solo en la pestana Juegos */
  st.pieCopiado = false;
  /* rama experimental: la primera (0) no es una pildora, la abre el logo y el nombre del programa; la de
     Rendimiento (2) ya no se ensena (sus filas van en la 3); la 3, la de antes «Programa», se llama
     «Ajustes». Con el nombre vacio las demas conservan su numero. */
  /* (v1.15) la 2, la de Rendimiento que ya no se ensenaba, es ahora Integraciones: queda entre Juegos y Ajustes */
  st.pestanas = {"", idioma::tr("Juegos"), idioma::tr("Integraciones"), idioma::tr("Ajustes"), "FG", idioma::tr("Efectos")};   /* (v1.18) y FG, y los efectos */
  st.ordenPestanas = {0, 1, 5, 2, 3, 4};   /* (v1.18) Efectos junto a Juegos (Rodolfo: «beside games, call it effects») */
  st.sinRedPanel = v.expSinRed == "si" || v.expSinRed == "suelo" || v.expSinRed == "imagen";
  st.items.clear();
  ids.clear();
  auto pon = [&](Hud::ItemPanel it, const std::string &id) {
    st.items.push_back(std::move(it));
    ids.push_back(id);
  };
  /* el boton junto al aspa: lo que se sabe de la version de GitHub */
  const Actualizacion act = estadoActualizacion();
  st.actualizacion = act.pendiente ? act.remota : std::string();
  st.alDia = act.alDia;
  /* (v1.16) las teselas del panel al estilo de vkDaedalus: debajo de las grandes (las pestanas), lo que tiene cada
     una; a los lados, los interruptores que mas se tocan, la version y el idioma */
  {
    const integ::Estado ie = integ::cargar();
    int conLoSuyo = 0;
    for (const Juego &j : juegos) {
      const auto it = ie.juegos.find(j.id);
      if (it != ie.juegos.end() && (!it->second.propios.empty() || it->second.propia)) conLoSuyo++;
    }
    const bool efectosTodos = efectos::cargar("").algoQueAplicar();   /* (v1.18) la nota de los efectos */
    st.pestanasNota = {
        idioma::trf("Relieve x{}", {conDecimal(v.relieve, 2)}),
        juegos.size() == 1 ? std::string(idioma::tr("1 juego")) : idioma::trf("{} juegos", {std::to_string(juegos.size())}),
        conLoSuyo == 0 ? std::string(idioma::tr("Todos igual")) : idioma::trf("{} con lo suyo", {std::to_string(conLoSuyo)}),
        st.actualizacion.empty() ? std::string("OdysseyVK ") + ODYSSEYVK_VERSION : idioma::trf("Hay la {}", {st.actualizacion}),
        notaFg(),   /* (v1.18) la de FG: el multiplicador del juego que corre (X2, X3, X4) o Apagada */
        std::string(idioma::tr(efectosTodos ? "Encendidos" : "Apagados"))};
    auto onOff = [](bool b) { return std::string(idioma::tr(b ? "Encendido" : "Apagado")); };
    st.teselas = {
        {"profundidad_dlss", idioma::tr("Profundidad de DLSS"), onOff(v.profDlss), v.profDlss, 1},
        {"hud_plano", idioma::tr("HUD del juego"), idioma::tr(v.hudPlano ? "En el plano" : "Con relieve"), v.hudPlano, 2},
        {"rastreo", idioma::tr("Seguir la cabeza"), onOff(v.rastreo), v.rastreo, 3},
        {"fps_juego", idioma::tr("Cuadrito de FPS"), onOff(v.fpsJuego), v.fpsJuego, 4},
        /* (v1.16) con una version nueva, como el boton que habia junto al aspa (Rodolfo, con una foto de el y de la
           tesela: «haz que esto ocurra aqui»): «Actualizar a 1.17», y la version de ahora en el titulo */
        {"buscar_actualizacion",
         st.actualizacion.empty() ? std::string(idioma::tr("Versión")) : idioma::trf("Versión {}", {ODYSSEYVK_VERSION}),
         st.actualizacion.empty() ? std::string(ODYSSEYVK_VERSION) + (st.alDia ? std::string(" · ") + idioma::tr("al día") : std::string())
                                  : idioma::trf("Actualizar a {}", {st.actualizacion}),
         false, 5},
        {"menu_idioma", idioma::tr("Idioma"), idioma::nombre(idioma::actual()), false, 6}};
    /* (v1.17) la del cuadrito de los FPS lleva su menu de lo que se vigila; si hay algo, lo dice */
    for (Hud::State::Tesela &t : st.teselas)
      if (t.id == "fps_juego") {
        t.menu = true;
        const size_t n = monitorElegido().size();
        if (v.fpsJuego && n > 0) t.valor = idioma::trf("{} · {} más", {t.valor, std::to_string(n)});
      }
  }
  /* (v1.16) la vista de depuracion: solo las dos listas de redes, en la columna de la izquierda (vkui.cpp) */
  if (st.depuracion && st.marco) {
    filasDeLaRed(st, v, ids);
    return;
  }
  if (st.pestana == 3) {
    /* Ajustes (antes Programa, v0.95): arriba el rendimiento (el visor 3D, la red y las tarjetas), luego
       la version, la actualizacion, otros y el desinstalador */
    filasRendimiento(st, v, ids);
    /* (v1.13) JUEGOS CON DLSS: la profundidad del juego, su HUD en el plano y el cuadrito de FPS («que en los
       settings se puedan desactivar», Rodolfo) */
    {
      Hud::ItemPanel sd;
      sd.tipo = Hud::ItemPanel::Seccion, sd.rotulo = idioma::tr("JUEGOS CON DLSS");
      pon(sd, "");
      Hud::ItemPanel pd;
      pd.tipo = Hud::ItemPanel::Interruptor, pd.rotulo = idioma::tr("Usar la profundidad de DLSS");
      pd.valor = v.profDlss;
      pd.ayuda = idioma::tr("Con ODYSSEYVK=1 en las opciones de lanzamiento del juego, el 3D toma la profundidad exacta que el juego le da a DLSS en vez de la red. Se aplica al momento; el modo Rendimiento no la usa nunca.");
      pon(pd, "profundidad_dlss");
      Hud::ItemPanel hp;
      hp.tipo = Hud::ItemPanel::Interruptor, hp.rotulo = idioma::tr("HUD del juego en el plano de la pantalla");
      hp.valor = v.hudPlano;
      hp.ayuda = idioma::tr("El minimapa, la munición, los textos y los menús del juego se quedan quietos en la pantalla, sin relieve.");
      pon(hp, "hud_plano");
      Hud::ItemPanel fj;
      fj.tipo = Hud::ItemPanel::Interruptor, fj.rotulo = idioma::tr("Cuadro de FPS junto a la pastilla");
      fj.valor = v.fpsJuego;
      fj.ayuda = idioma::tr("Los FPS del juego y, con DLSS, los de su capa, mientras haya 3D sobre el juego.");
      pon(fj, "fps_juego");
    }
    /* FLUIDEZ (rama experimental): lo que salio de Experimental. El VRR solo se elige con la red: en
       Rendimiento el 3D deja el del escritorio */
    {
      const bool sinRedPuesto = v.expSinRed == "si" || v.expSinRed == "suelo" || v.expSinRed == "imagen";
      Hud::ItemPanel fl;
      fl.tipo = Hud::ItemPanel::Seccion, fl.rotulo = idioma::tr("FLUIDEZ");
      pon(fl, "");
      if (!sinRedPuesto) {
        Hud::ItemPanel vr;
        vr.tipo = Hud::ItemPanel::Interruptor, vr.rotulo = idioma::tr("Frecuencia variable (VRR) durante el 3D");
        vr.valor = v.vrrEn3d;
        vr.ayuda = idioma::tr("De fábrica se apaga: con ella, los juegos por debajo del refresco daban tirones") + std::string(". ") +
                   idioma::tr("Con ella la pantalla sigue el ritmo del juego. En Cyberpunk, por debajo de 100 cuadros daba tirones (25 % de los cuadros tarde); sin ella, 165 Hz fijos.");
        pon(vr, "vrr_en_3d");
      }
      Hud::ItemPanel t;
      t.tipo = Hud::ItemPanel::Interruptor, t.rotulo = idioma::tr("Tope de arrastre");
      t.ayuda = idioma::tr("Donde la imagen se mueve, ningún punto se queda más de 1/16 de la escala por detrás del mapa nuevo: lo que se mueve no deja estela");
      t.valor = v.expTope;
      pon(t, "exp_tope_arrastre");
      /* la prioridad del visor en la tarjeta (salio de Experimental en la 1.12): con un juego llenando la
         tarjeta, el cuadro 3D no espera su turno */
      Hud::ItemPanel pr;
      pr.tipo = Hud::ItemPanel::Opciones, pr.rotulo = idioma::tr("Prioridad del visor en la tarjeta");
      pr.opciones = {idioma::tr("Normal"), idioma::tr("Alta"), idioma::tr("Tiempo real")};
      pr.elegido = v.expPrioridad == "alta" ? 1 : (v.expPrioridad == "tiempo_real" ? 2 : 0);
      pr.ayuda = idioma::tr("Con un juego llenando la tarjeta, el cuadro 3D pasa por delante del juego y no espera su turno; el juego pierde un poco.");
      pon(pr, "exp_prioridad");
      /* (v1.14) el tope de FPS de la capa de DLSS (Rodolfo: «en ajustes en fluidez añade una casilla de Layer FPS cap,
         y limitala como te digan»): 0 = sin tope, y de 10 a 240 de 5 en 5 */
      Hud::ItemPanel tc;
      tc.tipo = Hud::ItemPanel::Deslizador, tc.rotulo = idioma::tr("Tope de FPS de la capa");
      tc.ayuda = idioma::tr("Cuántas veces por segundo copia la capa de DLSS la profundidad y el HUD dentro del juego · 0 = sin tope. El visor hace 6 mapas por segundo de fábrica: con un tope, el juego gasta menos en copias que no se usan. Se aplica al momento.");
      tc.frac = (float)v.capaTope / 240.0f;
      tc.valorTexto = v.capaTope > 0 ? std::to_string(v.capaTope) + " fps" : std::string(idioma::tr("sin tope"));
      tc.textoFraccion = textoTopeCapa;
      pon(tc, "capa_fps_tope");
    }
    /* SEGUIMIENTO DE LA CABEZA (26-nov, «manda anticipar la cabeza a Ajustes»): salio de Experimental */
    {
      Hud::ItemPanel sc;
      sc.tipo = Hud::ItemPanel::Seccion, sc.rotulo = idioma::tr("SEGUIMIENTO DE LA CABEZA");
      pon(sc, "");
      Hud::ItemPanel c;
      c.tipo = Hud::ItemPanel::Interruptor, c.rotulo = idioma::tr("Seguir la cabeza con la cámara");
      c.ayuda = idioma::tr("Desde el siguiente vídeo"), c.valor = v.rastreo;
      pon(c, "rastreo");
      Hud::ItemPanel a;
      a.tipo = Hud::ItemPanel::Deslizador, a.rotulo = idioma::tr("Anticipar la cabeza");
      a.ayuda = idioma::tr("La trama apunta a donde estarán los ojos dentro de estos milisegundos · 0 = apagado") + std::string(". ") + idioma::tr("Al mover la cabeza, la cámara y la trama llegan tarde unos milisegundos; con esto la trama se calcula para donde estarán los ojos, siguiendo su velocidad. Any2Odyssey usa 25 ms; de más, la trama se pasa al frenar.");
      a.frac = (float)v.expAnticipacion / 60.0f;
      a.valorTexto = std::to_string(v.expAnticipacion) + " ms";
      a.textoFraccion = textoAnticipacion;
      a.fracDefecto = 0.0f;   /* (v1.16, PR #1) su flechita: a 0 ms, apagado */
      pon(a, "exp_anticipacion");
    }
    Hud::ItemPanel s;
    /* PROGRAMA (v1.12): la version y lo de GitHub en UNA fila con su boton (antes «Instalada», «En
       GitHub» y el boton, tres filas), las actualizaciones, el idioma y donde se ve OdysseyVK (antes
       OTROS); las explicaciones, en la ayuda al pasar el raton */
    s.tipo = Hud::ItemPanel::Seccion, s.rotulo = idioma::tr("PROGRAMA");
    pon(s, "");
    std::string enGitHub;
    if (comprobacionEnMarcha()) enGitHub = idioma::tr("comprobando…");
    else if (act.remota.empty()) enGitHub = idioma::tr("sin comprobar todavía");
    else {
      char cuando[64] = {0};
      const time_t t = (time_t)act.hora;
      std::strftime(cuando, sizeof cuando, "%H:%M", std::localtime(&t));
      enGitHub = idioma::trf(act.pendiente ? "OdysseyVK {} · hay una versión nueva (visto a las {})" : "OdysseyVK {} · estás al día (visto a las {})", {act.remota, cuando});
    }
    Hud::ItemPanel b;
    b.tipo = Hud::ItemPanel::Boton;
    b.rotulo = act.pendiente ? idioma::trf("Actualizar a OdysseyVK {}", {act.remota}) : std::string("OdysseyVK ") + ODYSSEYVK_VERSION;
    b.nota = idioma::trf("En GitHub: {}", {enGitHub});
    b.ayuda = act.pendiente ? std::string(idioma::tr("Se baja de GitHub, se compila, se instala (pide tu clave) y el servicio se reinicia; el vídeo o juego en 3D se reabre."))
                            : idioma::trf("Mira el repositorio {}", {std::string(kRepoGitHub).substr(8)});
    b.valorTexto = idioma::tr(comprobacionEnMarcha() ? "Comprobando…" : (act.pendiente ? "Actualizar" : "Buscar"));
    b.valor = !comprobacionEnMarcha();
    pon(b, act.pendiente ? "actualizar" : "buscar_actualizacion");
    Hud::ItemPanel a;
    a.tipo = Hud::ItemPanel::Interruptor, a.rotulo = idioma::tr("Avisar cuando haya una versión nueva");
    a.ayuda = idioma::tr("Una tarjeta en la orilla de la pantalla con el botón de actualizar; sin esto, solo el botón junto al aspa");
    a.valor = v.avisarActualizaciones;
    pon(a, "actualizaciones");
    Hud::ItemPanel au;
    au.tipo = Hud::ItemPanel::Interruptor, au.rotulo = idioma::tr("Actualizar automáticamente");
    au.ayuda = idioma::tr("En cuanto haya una versión nueva se instala sola (igual pide tu clave para instalar)");
    au.valor = v.actualizarAuto;
    pon(au, "actualizar_auto");
    /* el idioma (v1.07) */
    Hud::ItemPanel li;
    li.tipo = Hud::ItemPanel::Opciones, li.rotulo = idioma::tr("Idioma");
    for (int k = 0; k < idioma::kLenguas; k++) li.opciones.push_back(idioma::nombre(k));
    li.elegido = idioma::actual();
    pon(li, "idioma");
    /* (v1.02) si la ventana del visor 3D sale en la barra de tareas del panel y en Alt+Tab */
    Hud::ItemPanel bt;
    bt.tipo = Hud::ItemPanel::Interruptor, bt.rotulo = idioma::tr("Mostrar OdysseyVK en la barra de tareas");
    bt.ayuda = idioma::tr("En el gestor de tareas del panel de abajo: el visor 3D y estos ajustes mientras están abiertos");
    bt.valor = v.enBarraTareas;
    pon(bt, "barra_tareas");
    Hud::ItemPanel sw;
    sw.tipo = Hud::ItemPanel::Interruptor, sw.rotulo = idioma::tr("Mostrar OdysseyVK en la bandeja del sistema");
    sw.ayuda = idioma::tr("El icono junto al reloj y las notificaciones: clic abre estos ajustes, clic central los juegos");
    sw.valor = v.enBandeja;
    pon(sw, "bandeja");
    /* desinstalar va plegado, con su flechita (rama experimental): no queda a mano de un clic suelto */
    Hud::ItemPanel sp;
    sp.tipo = Hud::ItemPanel::Seccion, sp.rotulo = idioma::tr("DESINSTALAR"), sp.plegable = true;
    sp.valor = seccionesAbiertas().count("desinstalar") > 0;
    pon(sp, "plegar:desinstalar");
    if (!sp.valor) return;
    Hud::ItemPanel d;
    d.tipo = Hud::ItemPanel::Boton, d.rotulo = idioma::tr("Desinstalar OdysseyVK");
    d.nota = idioma::tr("Quita el programa, el servicio y la red de profundidad (pide tu clave); tu configuración se queda.");
    d.valorTexto = idioma::tr(g_confirmarDesinstalar > ::time(nullptr) ? "¿Seguro? Pica otra vez" : "Desinstalar");
    d.valor = true;
    d.peligro = true;
    pon(d, "desinstalar");
    return;
  }
  if (st.pestana == 0) {
    /* sin ninguna red de profundidad no hay video normal a 3D: lo primero que se ve es como bajarla; con alguna, */
    const bool sinRed = redPuesta(v).empty();   /* (v1.16) va despues de Pantalla */
    if (sinRed) filasDeLaRed(st, v, ids);
    Hud::ItemPanel s;
    s.tipo = Hud::ItemPanel::Seccion, s.rotulo = idioma::tr("VÍDEO");
    pon(s, "");
    /* (v1.12, «reacomoda las opciones que quedan para ser compactas y utiles»): el aviso de que el tipo de
       video se detecta solo ya no ocupa una fila; las explicaciones de segunda linea pasan a la ayuda que
       sale al pasar el raton, y el seguimiento de la cabeza se va a Ajustes, junto a la anticipacion */
    Hud::ItemPanel r;
    r.tipo = Hud::ItemPanel::Deslizador, r.rotulo = idioma::tr("Fuerza del relieve");
    r.ayuda = idioma::tr("Para el vídeo normal y el dividido suavizado · desde el siguiente vídeo");
    r.frac = (float)((v.relieve - 0.5) / 1.5), r.valorTexto = conDecimal(v.relieve, 2), r.textoFraccion = textoRelieve;
    pon(r, "relieve");
    Hud::ItemPanel d;
    d.tipo = Hud::ItemPanel::Opciones, d.rotulo = idioma::tr("Suavizar los vídeos divididos");
    d.opciones = {idioma::tr("Siempre"), idioma::tr("Si hace falta"), idioma::tr("Nunca")};
    d.elegido = v.suavizar == 1 ? 0 : (v.suavizar == 2 ? 1 : 2);
    d.ayuda = idioma::tr("Al ponerse a pantalla completa, el visor mira los primeros segundos (normal, lado a lado o arriba y abajo) y luego deja de mirar; en la pastilla se puede cambiar a mano.");
    pon(d, "dividido_suavizado");

    s.rotulo = idioma::tr("PANTALLA");
    pon(s, "");
    Hud::ItemPanel f;
    f.tipo = Hud::ItemPanel::Deslizador, f.rotulo = idioma::tr("Reducir el fantasma");
    f.ayuda = idioma::tr("Cuánta imagen del otro ojo se resta · se nota al momento");
    f.frac = (float)(v.fantasma / 0.10), f.valorTexto = conDecimal(v.fantasma * 100.0, 1) + " %", f.textoFraccion = textoFantasma;
    pon(f, "crosstalk_base");
    /* la pendiente del filtro de la trama (rama experimental, 26-nov), como parte del ciclo mezclada */
    Hud::ItemPanel mz;
    mz.tipo = Hud::ItemPanel::Deslizador, mz.rotulo = idioma::tr("Suavizar entre las vistas");
    mz.frac = (float)((v.mezclaVistas - 0.10) / 0.90);
    mz.valorTexto = std::to_string((int)std::lround(v.mezclaVistas * 100.0)) + " %";
    mz.textoFraccion = textoMezcla;
    mz.ayuda = idioma::tr("La lente reparte los subpíxeles entre tu ojo izquierdo y el derecho. Con el 10 % (lo de Samsung) el paso de una vista a la otra es casi un escalón: si el rastreo va un poco atrasado, una franja de la pantalla salta de golpe a la imagen del otro ojo. Más mezcla hace ese paso gradual; con demasiada, algo del otro ojo se ve aun con la cabeza quieta.");
    pon(mz, "filtro");

    if (!sinRed) filasDeLaRed(st, v, ids);
    /* ATAJOS DE TECLADO (v1.09): el interruptor de los globales y cada atajo, que se cambia picando
       su tecla y pulsando la combinacion nueva */
    /* las dos secciones largas se pliegan con una flechita (rama experimental); esperando un atajo,
       la de atajos se queda abierta */
    auto plegable = [&](const char *rotulo, const std::string &clave, bool forzada) {
      Hud::ItemPanel sp;
      sp.tipo = Hud::ItemPanel::Seccion, sp.rotulo = idioma::tr(rotulo), sp.plegable = true;
      sp.valor = forzada || seccionesAbiertas().count(clave) > 0;
      pon(sp, "plegar:" + clave);
      return sp.valor;
    };
    if (plegable("ATAJOS DE TECLADO", "atajos", !g_capturando.empty())) {
    Hud::ItemPanel a;
    a.tipo = Hud::ItemPanel::Interruptor, a.rotulo = idioma::tr("Atajos de teclado en todo el escritorio");
    a.nota = idioma::tr("Los atajos valen aunque el teclado lo tenga el vídeo o el juego"), a.valor = v.atajos;
    pon(a, "atajos_globales");
    const std::map<std::string, std::string> puestos = atajos::combos();
    for (const atajos::Def &d : atajos::lista()) {
      Hud::ItemPanel at;
      at.tipo = Hud::ItemPanel::Atajo, at.rotulo = idioma::tr(d.texto);
      at.valor = g_capturando == d.nombre;
      at.valorTexto = at.valor ? std::string(idioma::tr("Pulsa las teclas…")) : puestos.at(d.nombre);
      at.quitar = puestos.at(d.nombre) != d.defecto;
      pon(at, std::string("atajo:") + d.nombre);
    }
    }

    if (plegable("APLICACIONES QUE NO PREGUNTAN", "apps", false)) {
    if (v.recordadas.empty()) {
      Hud::ItemPanel n;
      n.tipo = Hud::ItemPanel::Nota, n.rotulo = idioma::tr("Ninguna: se pregunta siempre (o se sigue lo de arriba).");
      pon(n, "");
    }
    for (const auto &par : v.recordadas) {
      if (par.first.rfind("steam_app_", 0) == 0) continue;   /* los juegos se llevan en su pestana */
      Hud::ItemPanel fila;
      fila.tipo = Hud::ItemPanel::Fila, fila.rotulo = par.first, fila.valorTexto = nombreModo(par.second);
      fila.quitar = true;
      pon(fila, "auto:" + par.first);
    }
    }
  } else if (st.pestana == 2) {
    filasIntegraciones(st, juegos, ids);   /* (v1.15) las opciones de lanzamiento de Steam */
  } else if (st.pestana == 4) {
    filasFg(st, ids);   /* (v1.18) la generacion de cuadros de MAKO */
  } else if (st.pestana == 5) {
    filasEfectos(st, ids, juegos);   /* (v1.18) los efectos de ReShade */
  } else {
    Hud::ItemPanel s;
    s.tipo = Hud::ItemPanel::Seccion, s.rotulo = "STEAM";
    pon(s, "");
    if (juegos.empty()) {
      Hud::ItemPanel n;
      n.tipo = Hud::ItemPanel::Nota, n.rotulo = idioma::tr("No encontré juegos instalados en las bibliotecas de Steam.");
      pon(n, "");
    }
    for (const Juego &j : juegos) {
      Hud::ItemPanel fila;
      fila.tipo = Hud::ItemPanel::Juego, fila.rotulo = j.nombre;
      fila.img = j.cara, fila.imgW = j.caraW, fila.imgH = j.caraH;
      /* 3D salvo que este apuntado como «sin 3D»: su ventana se llama steam_app_<id> */
      fila.valor = juegoEn3D(v, j.id);
      /* (v1.15) el engranaje de la caratula (al pasar el raton) abre sus integraciones: su ayuda */
      fila.ayudas = {idioma::tr("Integraciones de este juego: sus opciones de lanzamiento de Steam"),
                     idioma::tr("Efectos de este juego (ReShade)")};   /* (v1.18) el icono de sus efectos */
      pon(fila, "juego:" + j.id);
    }
    Hud::ItemPanel n;
    n.tipo = Hud::ItemPanel::Nota;
    n.rotulo = idioma::tr("3D: el juego entra solo en 3D al ponerse a pantalla completa. 2D: se deja como está.");
    pon(n, "");
    /* (v1.14) la profundidad de DLSS se pide juego a juego, en Steam: el parametro va en medio del pie del panel,
       a la vista sin bajar por la lista (Rodolfo: «On the bottom of games tab let user know to add the steam
       parameter, offer a copy button for it» y «el parametro ahi, por el centro») */
    /* (v1.16) ya no hay boton Copiar: una casilla de ODYSSEYVK=1 para todos los juegos, con su recordatorio al pasar el
       raton y cuantos juegos la llevan a su manera (Rodolfo: «put a reminder to enable the option either per game or
       globally for DLSS depth, show it only after hovering, place the option there for the global setting but if any
       game has it removed remember the specific exclusions») */
    {
      const integ::Estado ie = integ::cargar();
      const bool todos = ie.todos.count("odysseyvk") && ie.todos.at("odysseyvk") == "1";
      int distintos = 0;
      for (const Juego &j : juegos)
        if (const auto it = ie.juegos.find(j.id); it != ie.juegos.end())
          if (const auto p = it->second.propios.find("odysseyvk"); p != it->second.propios.end() && (p->second == "1") != todos)
            distintos++;
      st.pieInterruptor = true;
      st.pieValor = todos;
      st.pieParametro = std::string(idioma::tr("ODYSSEYVK=1 en todos los juegos")) +
                        (distintos == 0 ? std::string()
                                        : " · " + idioma::trf(todos ? "{} sin ella" : "{} con ella", {std::to_string(distintos)}));
      st.pieAyuda = std::string(idioma::tr("Para la profundidad de DLSS, el juego necesita ODYSSEYVK=1 en sus opciones de lanzamiento de Steam.")) +
                    " " + idioma::tr("Aquí se pone o se quita para todos los juegos; uno a uno, con el engranaje de su carátula (Integraciones).") +
                    " " + idioma::tr("Los juegos que la llevan a su manera la siguen llevando así.");
    }
  }
}

/* (v1.16) en el panel al estilo de vkDaedalus las teselas de los lados ya llevan la profundidad de DLSS, el HUD, el
   seguimiento de la cabeza, el cuadrito de FPS, la version y el idioma: sus filas salen de la lista (Rodolfo: «quita
   las opciones que ya tengan boton de ajustes, no repitas opciones»), y con ellas el rotulo de una seccion que se
   quede sin nada debajo. En el panel de antes, sin teselas, se quedan */
static void llenarSinArrastre(Hud::State &st, const Valores &v, const std::vector<Juego> &juegos, std::vector<std::string> &ids);

/* (v1.16, de la PR #1 de Noa3) la lista se rehace (en Ajustes, cada segundo, por los datos de rendimiento) aunque se este
   arrastrando un deslizador: se conserva el que se tiene apretado, por su id (si salen o se van filas por encima,
   cambia de sitio) y con la bola donde esta. Antes volvia a lo guardado a media arrastrada */
void llenar(Hud::State &st, const Valores &v, const std::vector<Juego> &juegos, std::vector<std::string> &ids) {
  std::string idArrastre;
  float fracArrastre = 0.0f;
  if (st.arrastrando >= 0 && (size_t)st.arrastrando < ids.size() && (size_t)st.arrastrando < st.items.size())
    idArrastre = ids[(size_t)st.arrastrando], fracArrastre = st.items[(size_t)st.arrastrando].frac;
  st.arrastrando = -1;
  llenarSinArrastre(st, v, juegos, ids);
  if (idArrastre.empty()) return;
  for (size_t k = 0; k < st.items.size() && k < ids.size(); k++)
    if (ids[k] == idArrastre && st.items[k].tipo == Hud::ItemPanel::Deslizador) {
      st.arrastrando = (int)k;
      st.items[k].frac = fracArrastre;
      break;
    }
}

static void llenarSinArrastre(Hud::State &st, const Valores &v, const std::vector<Juego> &juegos, std::vector<std::string> &ids) {
  llenarFilas(st, v, juegos, ids);
  if (!st.marco || st.teselas.empty()) return;
  static const char *kEnTesela[] = {"profundidad_dlss", "hud_plano", "fps_juego", "rastreo", "idioma", "actualizar", "buscar_actualizacion"};
  std::vector<Hud::ItemPanel> items;
  std::vector<std::string> nids;
  for (size_t k = 0; k < st.items.size() && k < ids.size(); k++) {
    bool fuera = false;
    for (const char *e : kEnTesela) fuera = fuera || ids[k] == e;
    if (!fuera) items.push_back(st.items[k]), nids.push_back(ids[k]);
  }
  /* los rotulos de seccion sin filas debajo (salvo los plegables, que plegados no las tienen) */
  st.items.clear(), ids.clear();
  for (size_t k = 0; k < items.size(); k++) {
    const bool seccion = items[k].tipo == Hud::ItemPanel::Seccion && !items[k].plegable;
    const bool vacia = k + 1 >= items.size() || items[k + 1].tipo == Hud::ItemPanel::Seccion;
    if (seccion && vacia) continue;
    st.items.push_back(items[k]), ids.push_back(nids[k]);
  }
}

const std::string &capturando() { return g_capturando; }
void cancelarCaptura() { g_capturando.clear(); }

namespace {
const atajos::Def *defDe(const std::string &nombre) {
  for (const atajos::Def &d : atajos::lista())
    if (nombre == d.nombre) return &d;
  return nullptr;
}
}  // namespace

bool teclaCapturada(const std::string &combo0, std::string *aviso) {
  const atajos::Def *d = defDe(g_capturando);
  if (!d) return false;
  if (combo0 == "Escape") {
    g_capturando.clear();
    if (aviso) *aviso = idioma::tr("sin cambios");
    return true;
  }
  const std::string combo = atajos::normaliza(combo0);
  if (combo.empty()) return false;
  /* (v1.12) una tecla sola tambien vale (Rodolfo: «allow one key being assigned too»). Hasta la 1.11
     se seguia esperando: el atajo es global mientras hay 3D, asi que esa tecla deja de llegar al video
     o al juego de debajo; ahora se acepta y el pie lo avisa */
  const bool suelta = !atajos::tieneModificador(combo);
  for (const auto &par : atajos::combos())
    if (par.second == combo && par.first != d->nombre) {
      if (aviso) *aviso = idioma::trf("{} ya lo usa «{}»", {combo, idioma::tr(defDe(par.first)->texto)});
      return true;
    }
  std::string e;
  /* la de fabrica se guarda vacia: si un dia cambia la de fabrica, se sigue a la nueva */
  if (!ajuste::fijarEnServeConf(std::string("atajo_") + d->clave, combo == d->defecto ? "" : combo, &e)) {
    if (aviso) *aviso = idioma::trf("no se pudo guardar: {}", {e});
    g_capturando.clear();
    return true;
  }
  g_capturando.clear();
  avisarVisor({}, {{"atajos", "recargar"}});
  if (aviso)
    *aviso = idioma::trf(suelta ? "«{}» ahora va con {}: mientras haya 3D, esa tecla ya no llega al vídeo ni al juego"
                                : "«{}» ahora va con {}",
                         {idioma::tr(d->texto), combo});
  return true;
}

bool aplicar(const std::string &id, int sub, float frac, bool soltado, const std::vector<Juego> &juegos,
             std::string *aviso, bool *cerrar) {
  std::string e;
  auto fija = [&](const std::string &k, const std::string &val) {
    if (ajuste::fijarEnServeConf(k, val, &e)) return true;
    if (aviso) *aviso = idioma::trf("no se pudo guardar: {}", {e});
    return false;
  };
  /* plegar o desplegar una seccion de Ajustes: se apunta y el panel se vuelve a llenar */
  if (id.rfind("plegar:", 0) == 0) {
    std::set<std::string> &ab = seccionesAbiertas();
    const std::string clave = id.substr(7);
    if (!ab.erase(clave)) ab.insert(clave);
    if (FILE *f = std::fopen(rutaEstadoOdyssey("panel-secciones").c_str(), "we")) {
      for (const std::string &c : ab) std::fprintf(f, "%s\n", c.c_str());
      std::fclose(f);
    }
    return true;
  }
  if (id.rfind("integ:", 0) == 0) return aplicarIntegracion(id.substr(6), sub, juegos, aviso);
  if (id.rfind("fg:", 0) == 0) return aplicarFg(id.substr(3), sub, aviso);   /* (v1.18) la pestana FG */
  if (id.rfind("fgj:", 0) == 0) {   /* (v1.18) el perfil de MAKO de un juego, desde sus Integraciones: «fgj:que:exe» */
    const size_t dos = id.find(':', 4);
    if (dos == std::string::npos) return false;
    return aplicarPerfilMako(id.substr(dos + 1), id.substr(4, dos - 4), sub, aviso);
  }
  /* (v1.15) el engranaje de una caratula: las Integraciones de ese juego */
  /* (v1.18) el icono de efectos de una caratula: los efectos de ese juego, en su pestana */
  if (id.rfind("juego:", 0) == 0 && sub == 3) {
    g_efectosDe = id.substr(6), g_efectosDesdeJuegos = true;
    g_pestanaPedida = 5;
    return true;
  }
  if (id.rfind("haz:", 0) == 0) return aplicarHaz(id.substr(4), sub, frac, aviso);   /* (v1.18) ShaderBeam */
  if (id.rfind("efx:", 0) == 0) return aplicarEfectos(id.substr(4), sub, frac, aviso);
  if (id.rfind("juego:", 0) == 0 && sub == 2) {
    g_integJuego = id.substr(6);
    g_pestanaPedida = 2;
    return true;
  }
  const Valores v = leer();
  if (id == "modo") {
    static const char *kModo[] = {"ask", "conv", "sbs"};
    if (sub < 0 || sub > 2 || !fija("modo", kModo[sub])) return false;
    if (aviso) *aviso = idioma::tr(sub == 0 ? "se preguntará con la tarjeta" : "desde el siguiente vídeo, sin preguntar");
    return true;
  }
  if (id == "dividido_suavizado") {
    static const char *kVal[] = {"si", "auto", "no"};
    static const char *kDice[] = {"los vídeos divididos se suavizan siempre",
                                  "con su propio relieve mientras quepa; suavizado solo si algo sale doble",
                                  "los vídeos divididos se ven siempre con su propio relieve"};
    if (sub < 0 || sub > 2 || !fija(id, kVal[sub])) return false;
    avisarVisor({}, {{"dividido_suavizado", kVal[sub]}});
    if (aviso) *aviso = idioma::tr(kDice[sub]);
    return true;
  }
  if (id == "rastreo" || id == "atajos_globales") {
    const bool ahora = id == "rastreo" ? v.rastreo : v.atajos;
    if (!fija(id, ahora ? "no" : "si")) return false;
    if (aviso) *aviso = idioma::tr("se aplica desde el siguiente vídeo");
    return true;
  }
  if (id == "filtro") {
    const double m = mezclaDe(frac);
    const double pendiente = 1.0 / m;
    avisarVisor({{"filtro", pendiente}}, {});   /* se ve al momento, tambien mientras se arrastra */
    if (!soltado) return false;
    if (!fija(id, conComa(pendiente, 2))) return false;
    if (aviso) *aviso = idioma::trf("el {} % del paso entre las vistas se mezcla", {std::to_string((int)std::lround(m * 100.0))});
    return true;
  }
  if (id == "crosstalk_base") {
    const double val = fantasmaDe(frac);
    avisarVisor({{"crosstalk_base", val}}, {});   /* se ve al momento, tambien mientras se arrastra */
    if (!soltado) return false;
    if (!fija(id, conComa(val, 4))) return false;
    if (aviso) *aviso = idioma::trf("se resta el {} % de la imagen del otro ojo", {conDecimal(val * 100.0, 1)});
    return true;
  }
  if (id == "relieve") {
    if (!soltado) return false;
    const double val = relieveDe(frac);
    if (!fija(id, conComa(val, 2))) return false;
    if (aviso) *aviso = idioma::trf("fuerza del relieve {} · desde el siguiente vídeo", {conDecimal(val, 2)});
    return true;
  }
  if (id.rfind("atajo:", 0) == 0) {
    const atajos::Def *d = defDe(id.substr(6));
    if (!d) return false;
    if (sub == 1) {   /* la flecha: volver a la de fabrica */
      g_capturando.clear();
      if (!fija(std::string("atajo_") + d->clave, "")) return false;
      avisarVisor({}, {{"atajos", "recargar"}});
      if (aviso) *aviso = idioma::trf("«{}» vuelve a {}", {idioma::tr(d->texto), d->defecto});
      return true;
    }
    if (g_capturando == d->nombre) {   /* picar otra vez la misma tecla: dejar de esperar */
      g_capturando.clear();
      if (aviso) *aviso = idioma::tr("sin cambios");
      return true;
    }
    g_capturando = d->nombre;
    if (aviso) *aviso = idioma::trf("Pulsa la combinación nueva para «{}» · Esc cancela", {idioma::tr(d->texto)});
    return true;
  }
  if (id == "exp_sin_red") {
    static const char *kVal[] = {"no", "si"};
    if (sub < 0 || sub > 1 || !fija("exp_sin_red", kVal[sub])) return false;
    if (aviso) *aviso = idioma::tr("se aplica desde el siguiente vídeo");
    return true;
  }
  /* (v1.13) los juegos con DLSS */
  if (id == "profundidad_dlss" || id == "hud_plano") {
    const bool ahora = id == "profundidad_dlss" ? v.profDlss : v.hudPlano;
    if (!fija(id, ahora ? "no" : "si")) return false;
    avisarVisor({}, {{id, ahora ? "no" : "si"}});   /* (v1.18) al momento: el visor vuelve a la red o toma la de DLSS */
    return true;
  }
  if (id == "capa_fps_tope") {
    if (!soltado) return false;
    const int n = topeCapaDe(frac);
    if (!fija("capa_fps_tope", std::to_string(n))) return false;
    avisarVisor({}, {{"capa_fps_tope", std::to_string(n)}});   /* al momento: el visor se lo pasa a la capa */
    if (aviso)
      *aviso = idioma::trf("tope de la capa: {}", {n > 0 ? std::to_string(n) + " fps" : std::string(idioma::tr("sin tope"))});
    return true;
  }
  if (id == "fps_juego") {
    if (!fija("fps_juego", v.fpsJuego ? "no" : "si")) return false;
    avisarVisor({}, {{"fps_juego", v.fpsJuego ? "no" : "si"}});   /* se ve al momento */
    return true;
  }
  if (id == "vrr_en_3d") {
    if (!fija("vrr_en_3d", v.vrrEn3d ? "no" : "si")) return false;
    if (aviso) *aviso = idioma::tr("se aplica desde el siguiente vídeo");
    return true;
  }
  if (id == "exp_prioridad") {
    static const char *kVal[] = {"normal", "alta", "tiempo_real"};
    if (sub < 0 || sub > 2 || !fija("exp_prioridad", kVal[sub])) return false;
    if (aviso) *aviso = idioma::tr("se aplica desde el siguiente vídeo");
    return true;
  }
  if (id == "exp_tope_arrastre") {
    if (!fija("exp_tope_arrastre", v.expTope ? "no" : "si")) return false;
    if (aviso) *aviso = idioma::tr("se aplica desde el siguiente vídeo");
    return true;
  }
  if (id == "exp_anticipacion") {
    if (!soltado) return false;
    const int ms = anticipacionDe(frac);
    if (!fija("exp_anticipacion", std::to_string(ms))) return false;
    if (aviso) *aviso = idioma::trf("anticipación {} ms · desde el siguiente vídeo", {std::to_string(ms)});
    return true;
  }
  if (id == "idioma") {
    /* el idioma de la interfaz (v1.07): se guarda, se pone en este proceso (el panel rehace su atlas
       de letras) y se le manda al visor si hay uno abierto; el servicio lo ve en su proxima mirada */
    if (sub < 0 || sub >= idioma::kLenguas) return false;
    if (sub == idioma::actual()) return false;
    if (!fija("idioma", idioma::codigo(sub))) return false;
    idioma::poner(sub);
    avisarVisor({}, {{"idioma", idioma::codigo(sub)}});
    if (aviso) *aviso = std::string(idioma::tr("Idioma")) + ": " + idioma::nombre(sub) + " · " + idioma::tr("el idioma cambia al momento");
    return true;
  }
  if (id == "actualizaciones") {
    if (!fija("actualizaciones", v.avisarActualizaciones ? "no" : "si")) return false;
    if (aviso) *aviso = idioma::tr(v.avisarActualizaciones ? "no se avisará de versiones nuevas; el botón junto al aspa las sigue marcando"
                                                           : "se avisará con una tarjeta en la orilla de la pantalla");
    return true;
  }
  if (id == "barra_tareas" || id == "bandeja") {
    const bool ahora = id == "barra_tareas" ? v.enBarraTareas : v.enBandeja;
    if (!fija(id, ahora ? "no" : "si")) return false;
    if (aviso) *aviso = id == "barra_tareas" ? idioma::tr(ahora ? "OdysseyVK ya no sale en la barra de tareas" : "OdysseyVK vuelve a salir en la barra de tareas")
                                             : idioma::tr(ahora ? "sin icono en la bandeja del sistema" : "con icono en la bandeja del sistema");
    return true;
  }
  if (id == "actualizar_auto") {
    if (!fija("actualizar_auto", v.actualizarAuto ? "no" : "si")) return false;
    if (aviso) *aviso = idioma::tr(v.actualizarAuto ? "las versiones nuevas ya no se instalan solas" : "las versiones nuevas se instalarán solas");
    return true;
  }
  if (id == "buscar_actualizacion") {
    if (!comprobarActualizacion(&e)) {
      if (aviso) *aviso = e;
      return false;
    }
    if (aviso) *aviso = idioma::tr("mirando la versión de GitHub…");
    return true;
  }
  if (id == "actualizar") {
    if (!lanzarActualizacion(&e)) {
      if (aviso) *aviso = e;
      return false;
    }
    if (aviso) *aviso = idioma::tr("actualizando: se compila, se pide tu clave para instalar y el servicio se reinicia");
    if (cerrar) *cerrar = true;
    return true;
  }
  if (id == "desinstalar") {
    const long ahora = (long)::time(nullptr);
    if (g_confirmarDesinstalar <= ahora) {
      g_confirmarDesinstalar = ahora + 12;
      if (aviso) *aviso = idioma::tr("¿seguro? pica otra vez en los próximos 12 segundos para desinstalar");
      return true;
    }
    g_confirmarDesinstalar = 0;
    if (!lanzarDesinstalacion(&e)) {
      if (aviso) *aviso = e;
      return false;
    }
    if (aviso) *aviso = idioma::tr("desinstalando OdysseyVK…");
    if (cerrar) *cerrar = true;
    return true;
  }
  /* (v1.16) la casilla de una red: picar una bajada la pone (una sola puesta; la puesta no se quita sola) */
  if (id.rfind("red:", 0) == 0) {
    const std::string ruta = id.substr(4);
    if (ruta == redPuesta(v)) {
      if (aviso) *aviso = idioma::tr("siempre va una puesta: marca otra para cambiar");
      return true;
    }
    return ponerRed(ruta, aviso);
  }
  /* (v1.16) la casilla de una red de video: se pone o se quita (sin ninguna, los videos van con la de siempre) */
  if (id.rfind("redvid:", 0) == 0) {
    const std::string ruta = id.substr(7);
    return ponerRedVideo(ruta == v.modeloVideo ? std::string() : ruta, aviso);
  }
  if (id.rfind("red_bajar:", 0) == 0) {
    const int cn = std::atoi(id.c_str() + 10), cual = cn == 1 || cn == 2 ? cn : 0;
    if (descargaEnMarcha()) {
      if (aviso) *aviso = idioma::tr("ya se está bajando una red: espera a que acabe");
      return true;
    }
    if (!descargar(cual, &e)) {
      if (aviso) *aviso = e;
      return false;
    }
    if (aviso)
      *aviso = idioma::tr(cual == 2 ? "bajando la red de vídeo (117 MB): al acabar queda puesta para los vídeos"
                                    : (cual == 1 ? "bajando la FP32 (99 MB): al acabar queda puesta" : "bajando la FP16 (50 MB): al acabar queda puesta"));
    return true;
  }
  if (id == "descargar_red") {
    if (!descargar(&e)) {
      if (aviso) *aviso = e;
      return false;
    }
    if (aviso) *aviso = idioma::tr("bajando la red abierta (50 MB)…");
    return true;
  }
  if (id.rfind("auto:", 0) == 0) {
    if (!ajuste::fijarAutoEnServeConf(id.substr(5), std::string(), &e)) {
      if (aviso) *aviso = idioma::trf("no se pudo guardar: {}", {e});
      return false;
    }
    if (aviso) *aviso = idioma::trf("se volverá a preguntar por {}", {id.substr(5)});
    return true;
  }
  if (id.rfind("juego:", 0) == 0) {
    const std::string ap = id.substr(6);
    const bool en3d = juegoEn3D(v, ap);
    if (sub == 1) {
      /* la pastillita: ese juego entra en 3D solo (conversor) o se deja en 2D sin preguntar */
      if (!ajuste::fijarAutoEnServeConf("steam_app_" + ap, en3d ? "flat" : "conv", &e)) {
        if (aviso) *aviso = idioma::trf("no se pudo guardar: {}", {e});
        return false;
      }
      if (aviso) *aviso = idioma::tr(en3d ? "ese juego se queda en 2D" : "ese juego entra solo en 3D");
      return true;
    }
    /* al arrancarlo se apunta lo que diga su pastillita, para que el servicio no pregunte */
    ajuste::fijarAutoEnServeConf("steam_app_" + ap, en3d ? "conv" : "flat", &e);
    std::string nombre = ap;
    for (const Juego &j : juegos)
      if (j.id == ap) nombre = j.nombre;
    if (!lanzarJuego(ap, &e)) {
      if (aviso) *aviso = e;
      return false;
    }
    if (aviso) *aviso = idioma::trf("arrancando {}…", {nombre});
    if (cerrar) *cerrar = true;
    return true;
  }
  return false;
}

/* (v1.14) el boton Copiar del pie de la pestana Juegos: el parametro de Steam al portapapeles del escritorio */
bool copiarParametroSteam(std::string *err) { return copiarAlPortapapeles(kParametroSteam, err); }

bool alternarOdysseyTodos(const std::vector<Juego> &juegos, std::string *aviso) {
  std::vector<std::string> ids;
  for (const Juego &j : juegos) ids.push_back(j.id);
  integ::Estado e = integ::cargar();
  std::map<std::string, std::string> steam;
  if (integ::leerSteam(steam, nullptr)) integ::sincronizar(e, ids, steam);   /* lo cambiado por fuera, antes */
  const bool ahora = e.todos.count("odysseyvk") && e.todos.at("odysseyvk") == "1";
  /* un juego con ODYSSEYVK=1 propio (lo tomado de su linea de Steam la primera vez, casi siempre) sigue a lo de
     todos: si no, apagarlo aqui no lo quitaria de ninguno. Los que la tienen quitada a su manera (=0) son las
     exclusiones y se quedan asi, encendida o apagada para todos (Rodolfo: «if any game has it removed remember the
     specific exclusions») */
  for (auto &[id, j] : e.juegos)
    if (const auto p = j.propios.find("odysseyvk"); p != j.propios.end() && p->second == "1") j.propios.erase(p);
  integ::fijar(e, "", "odysseyvk", ahora ? "0" : "1");
  std::string err;
  if (!integ::guardar(e, &err)) {
    if (aviso) *aviso = idioma::trf("no se pudo guardar: {}", {err});
    return false;
  }
  if (aviso) *aviso = dichoDe(integ::aplicar(ids, true));
  return true;
}

}  /* namespace prefs */
