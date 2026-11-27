/* preferencias.cpp — ver preferencias.h. */
#include "preferencias.h"

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
  v.modo = (modo == "conv" || modo == "sbs" || modo == "suave") ? (modo == "suave" ? "sbs" : modo) : "ask";
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
  for (std::string *m : {&v.modelo, &v.nuestra})
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

namespace {
bool tamanoEs(const std::string &ruta, long bytes) {
  struct stat sb {};
  return ::stat(ruta.c_str(), &sb) == 0 && sb.st_size == bytes;
}
}  // namespace

bool redLigeraPresente() { return tamanoEs(rutaRedLigera(), kBytesRedAbierta); }
bool redFp32Presente() { return tamanoEs(rutaRedVieja(), kBytesRedVieja); }
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
  return cual == 1 ? kUrlRedFp32 : kUrlRedAbierta;
}
std::string destinoDe(int cual) { return cual == 1 ? rutaRedVieja() : rutaRedLigera(); }
long bytesDe(int cual) { return cual == 1 ? kBytesRedVieja : kBytesRedAbierta; }
std::string megasDe(int cual) { return cual == 1 ? "99" : "50"; }
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
  cual = cual == 1 ? 1 : 0;
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
    g_descarga.texto = idioma::tr(cual == 1 ? "lista: la FP32 ya está en su carpeta" : "lista: la red abierta ya está en su carpeta");
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
    otras.push_back({e.path().string(), nombre.substr(0, nombre.size() - 5), 0, true, (long)e.file_size(e2)});
  }
  /* y la de `modelo =` si esta fuera de la carpeta */
  if (!v.modelo.empty() && fileExists(v.modelo)) {
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
   hace falta. */
void filasDeLaRed(Hud::State &st, const Valores &v, std::vector<std::string> &ids) {
  const std::vector<RedDisco> redes = redesDelDisco(v);
  const std::string puesta = redPuesta(v);
  auto pon = [&](Hud::ItemPanel it, const std::string &id) {
    st.items.push_back(std::move(it));
    ids.push_back(id);
  };
  Hud::ItemPanel s;
  s.tipo = Hud::ItemPanel::Seccion, s.rotulo = idioma::tr(puesta.empty() ? "FALTA LA RED DE PROFUNDIDAD" : "RED DE PROFUNDIDAD");
  pon(s, "");
  if (puesta.empty()) {
    Hud::ItemPanel i;
    i.tipo = Hud::ItemPanel::Info;
    i.rotulo = idioma::tr("Sin ella no hay datos de profundidad: el vídeo normal no se puede pasar a 3D (el dividido sí).");
    i.nota = idioma::tr("Depth Anything V2 pequeña, de huggingface.co/onnx-community/depth-anything-v2-small: la FP16 es la recomendada.");
    pon(i, "");
  }
  const int bajando = descargaCual();
  for (const RedDisco &r : redes) {
    Hud::ItemPanel it;
    it.tipo = Hud::ItemPanel::Interruptor;
    it.rotulo = r.conocida == 1 ? std::string(idioma::tr("Depth Anything V2 pequeña · FP16 (recomendada)"))
                                : (r.conocida == 2 ? std::string(idioma::tr("Depth Anything V2 pequeña · FP32")) : r.nombre);
    it.nota = r.conocida == 1 ? std::string(idioma::tr("La misma red a media precisión: el mismo mapa con un 35 % menos de tiempo y 260 MB menos en la tarjeta."))
              : r.conocida == 2 ? std::string(idioma::tr("La misma red a precisión completa: más lenta y con más memoria de la tarjeta."))
                                : idioma::trf("{} · {} MB", {r.ruta.rfind(carpetaRedes(), 0) == 0 ? r.ruta.substr(carpetaRedes().size()) : r.ruta,
                                                             std::to_string(std::max(1L, r.bytes / 1000000))});
    it.valor = r.presente && r.ruta == puesta;
    if (r.presente) {
      it.pastilla = idioma::tr(r.conocida ? "Descargada" : "En la carpeta");
      it.pastillaAcento = true;
      pon(it, "red:" + r.ruta);
    } else {
      const int cual = r.conocida == 2 ? 1 : 0;
      it.apagado = true;
      it.descargable = bajando < 0;   /* una descarga a la vez: mientras baja una, la otra no se pica */
      it.pastilla = bajando == cual ? textoDescarga() : idioma::trf("Descargar · {} MB", {cual == 1 ? "99" : "50"});
      pon(it, "red_bajar:" + std::to_string(cual));
    }
  }
  Hud::ItemPanel n;
  n.tipo = Hud::ItemPanel::Nota;
  n.rotulo = idioma::trf("Cualquier red de profundidad .onnx que pongas en {} empieza a salir en esta lista.",
                         {carpetaRedes().substr(0, carpetaRedes().size() - 1)});
  pon(n, "");
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
  if (!alguna) {
    Hud::ItemPanel n;
    n.tipo = Hud::ItemPanel::Nota, n.rotulo = idioma::tr("nvidia-smi no respondió: no hay datos de las tarjetas.");
    pon(n, "");
  }
}
}  // namespace

/* ---- la pestana Integraciones (v1.15) ------------------------------------------------------------------------- */
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
                      {"vkbasalt", "ENABLE_VKBASALT", "Los filtros de vkBasalt"}};
  for (const Var &v : vars) {
    const bool puesto = val(v.clave) == "1";
    const size_t k = interruptor(v.clave, std::string(v.nombre) + (puesto ? "=1" : ""), idioma::tr(v.ayuda));
    /* de un juego: el ejecutable que va con Neural Forge (no se pregunta: se saca de Neural Forge) */
    if (!jid.empty() && puesto && std::strcmp(v.clave, "neural_forge") == 0) {
      const std::string exe = integ::exeDe(e, jid);
      if (!exe.empty()) st.items[k].nota = "NEURAL_FORGE_TARGET_EXE=" + exe;
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
    l.tipo = Hud::ItemPanel::Linea, l.valorTexto = integ::linea(ef, jid.empty() ? std::string() : integ::exeDe(e, jid));
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
                               : integ::linea(integ::efectivo(e, j.id), integ::exeDe(e, j.id));
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
    const std::string l = integ::linea(ef, g_integJuego.empty() ? std::string() : integ::exeDe(e, g_integJuego));
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

std::string firmaIntegraciones() { return integ::firma() + (ahoraSeg() < g_copiadoHasta ? "|copiado" : ""); }

void probarEdicion(const std::string &clave, const std::string &texto) {
  g_edit = Edicion{};
  g_edit.id = "integ:" + clave, g_edit.texto = texto, g_edit.cursor = texto.size();
}

static void llenarFilas(Hud::State &st, const Valores &v, const std::vector<Juego> &juegos, std::vector<std::string> &ids) {
  st.pieParametro.clear(), st.pieInterruptor = false;   /* (v1.14) el parametro de Steam del pie: solo en la pestana Juegos */
  st.pieCopiado = false;
  /* rama experimental: la primera (0) no es una pildora, la abre el logo y el nombre del programa; la de
     Rendimiento (2) ya no se ensena (sus filas van en la 3); la 3, la de antes «Programa», se llama
     «Ajustes». Con el nombre vacio las demas conservan su numero. */
  /* (v1.15) la 2, la de Rendimiento que ya no se ensenaba, es ahora Integraciones: queda entre Juegos y Ajustes */
  st.pestanas = {"", idioma::tr("Juegos"), idioma::tr("Integraciones"), idioma::tr("Ajustes")};   /* la de Experimental se fue en la 1.12 */
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
    st.pestanasNota = {
        idioma::trf("Relieve x{}", {conDecimal(v.relieve, 2)}),
        juegos.size() == 1 ? std::string(idioma::tr("1 juego")) : idioma::trf("{} juegos", {std::to_string(juegos.size())}),
        conLoSuyo == 0 ? std::string(idioma::tr("Todos igual")) : idioma::trf("{} con lo suyo", {std::to_string(conLoSuyo)}),
        st.actualizacion.empty() ? std::string("OdysseyVK ") + ODYSSEYVK_VERSION : idioma::trf("Hay la {}", {st.actualizacion})};
    auto onOff = [](bool b) { return std::string(idioma::tr(b ? "Encendido" : "Apagado")); };
    st.teselas = {
        {"profundidad_dlss", idioma::tr("Profundidad de DLSS"), onOff(v.profDlss), v.profDlss, 1},
        {"hud_plano", idioma::tr("HUD del juego"), idioma::tr(v.hudPlano ? "En el plano" : "Con relieve"), v.hudPlano, 2},
        {"rastreo", idioma::tr("Seguir la cabeza"), onOff(v.rastreo), v.rastreo, 3},
        {"fps_juego", idioma::tr("Cuadrito de FPS"), onOff(v.fpsJuego), v.fpsJuego, 4},
        {"buscar_actualizacion", idioma::tr("Versión"),
         st.actualizacion.empty() ? std::string(ODYSSEYVK_VERSION) + (st.alDia ? std::string(" · ") + idioma::tr("al día") : std::string())
                                  : idioma::trf("{}, hay la {}", {ODYSSEYVK_VERSION, st.actualizacion}),
         !st.actualizacion.empty(), 5},
        {"menu_idioma", idioma::tr("Idioma"), idioma::nombre(idioma::actual()), false, 6}};
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
      pd.ayuda = idioma::tr("Con ODYSSEYVK=1 en las opciones de lanzamiento del juego, el 3D toma la profundidad exacta que el juego le da a DLSS en vez de la red.");
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
    r.frac = (float)((v.relieve - 0.5) / 1.5), r.valorTexto = conDecimal(v.relieve, 2);
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
    f.frac = (float)(v.fantasma / 0.10), f.valorTexto = conDecimal(v.fantasma * 100.0, 1) + " %";
    pon(f, "crosstalk_base");
    /* la pendiente del filtro de la trama (rama experimental, 26-nov), como parte del ciclo mezclada */
    Hud::ItemPanel mz;
    mz.tipo = Hud::ItemPanel::Deslizador, mz.rotulo = idioma::tr("Suavizar entre las vistas");
    mz.frac = (float)((v.mezclaVistas - 0.10) / 0.90);
    mz.valorTexto = std::to_string((int)std::lround(v.mezclaVistas * 100.0)) + " %";
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
      fila.valor = true;
      for (const auto &par : v.recordadas)
        if (par.first == "steam_app_" + j.id && par.second == "flat") fila.valor = false;
      /* (v1.15) el engranaje de la caratula (al pasar el raton) abre sus integraciones: su ayuda */
      fila.ayudas = {idioma::tr("Integraciones de este juego: sus opciones de lanzamiento de Steam")};
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
void llenar(Hud::State &st, const Valores &v, const std::vector<Juego> &juegos, std::vector<std::string> &ids) {
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
  /* (v1.15) el engranaje de una caratula: las Integraciones de ese juego */
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
    const double m = std::round((0.10 + std::max(0.0f, std::min(1.0f, frac)) * 0.90) / 0.05) * 0.05;
    const double pendiente = 1.0 / m;
    avisarVisor({{"filtro", pendiente}}, {});   /* se ve al momento, tambien mientras se arrastra */
    if (!soltado) return false;
    if (!fija(id, conComa(pendiente, 2))) return false;
    if (aviso) *aviso = idioma::trf("el {} % del paso entre las vistas se mezcla", {std::to_string((int)std::lround(m * 100.0))});
    return true;
  }
  if (id == "crosstalk_base") {
    const double val = std::round(std::max(0.0f, std::min(1.0f, frac)) * 0.10 / 0.002) * 0.002;
    avisarVisor({{"crosstalk_base", val}}, {});   /* se ve al momento, tambien mientras se arrastra */
    if (!soltado) return false;
    if (!fija(id, conComa(val, 4))) return false;
    if (aviso) *aviso = idioma::trf("se resta el {} % de la imagen del otro ojo", {conDecimal(val * 100.0, 1)});
    return true;
  }
  if (id == "relieve") {
    if (!soltado) return false;
    const double val = std::round((0.5 + std::max(0.0f, std::min(1.0f, frac)) * 1.5) / 0.05) * 0.05;
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
    if (aviso) *aviso = idioma::tr("se aplica desde el siguiente vídeo");
    return true;
  }
  if (id == "capa_fps_tope") {
    if (!soltado) return false;
    int n = (int)std::lround(std::max(0.0f, std::min(1.0f, frac)) * 48.0f) * 5;
    if (n > 0 && n < 10) n = 10;
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
    const int ms = (int)std::lround(std::max(0.0f, std::min(1.0f, frac)) * 60.0f);
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
  if (id.rfind("red_bajar:", 0) == 0) {
    const int cual = std::atoi(id.c_str() + 10) == 1 ? 1 : 0;
    if (descargaEnMarcha()) {
      if (aviso) *aviso = idioma::tr("ya se está bajando una red: espera a que acabe");
      return true;
    }
    if (!descargar(cual, &e)) {
      if (aviso) *aviso = e;
      return false;
    }
    if (aviso) *aviso = idioma::tr(cual == 1 ? "bajando la FP32 (99 MB): al acabar queda puesta" : "bajando la FP16 (50 MB): al acabar queda puesta");
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
    bool en3d = true;
    for (const auto &par : v.recordadas)
      if (par.first == "steam_app_" + ap && par.second == "flat") en3d = false;
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
