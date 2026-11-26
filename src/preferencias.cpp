/* preferencias.cpp — ver preferencias.h. */
#include "preferencias.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <map>
#include <system_error>

#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "ajuste.h"
#include "util.h"

#ifdef HAVE_OPENCV
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
#ifdef HAVE_OPENCV
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
  cv::Mat m = cv::imread(ruta, cv::IMREAD_COLOR);
  if (m.empty() || m.cols < 16 || m.rows < 16) return;
  cv::Mat chico, rgba;
  cv::resize(m, chico, cv::Size(280, 420), 0, 0, cv::INTER_AREA);
  cv::cvtColor(chico, rgba, cv::COLOR_BGR2RGBA);
  auto px = std::make_shared<std::vector<uint8_t>>((size_t)rgba.cols * rgba.rows * 4u);
  for (int y = 0; y < rgba.rows; y++) std::memcpy(px->data() + (size_t)y * rgba.cols * 4u, rgba.ptr(y), (size_t)rgba.cols * 4u);
  j.cara = px;
  j.caraW = rgba.cols, j.caraH = rgba.rows;
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

const char *nombreModo(const std::string &m) {
  if (m == "conv") return "vídeo normal";
  if (m == "sbs" || m == "suave") return "vídeo dividido";
  if (m == "tb") return "arriba y abajo";
  if (m == "flat") return "sin 3D";
  return m.c_str();
}

/* avisar al visor abierto (si lo hay): una orden suelta por el canal de ajuste */
void avisarVisor(const std::map<std::string, double> &numeros, const std::map<std::string, std::string> &palabras) {
  std::string e;
  ajuste::escribir(numeros, std::string(), palabras, &e);
}

}  // namespace

std::vector<Juego> juegosSteam() {
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
      cargaCaratula(raices.front(), j);
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
      if (err) *err = "ese no es un juego de Steam";
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
  v.hdr = si(de("hdr_en_3d"), false);
  v.atajos = si(de("atajos_globales"), true);
  v.fantasma = std::max(0.0, std::min(0.10, numero(de("crosstalk_base"), 0.0128535)));
  v.relieve = std::max(0.5, std::min(2.0, numero(de("relieve"), 1.0)));
  v.modelo = de("modelo");
  v.avisarActualizaciones = si(de("actualizaciones"), true);
  v.actualizarAuto = si(de("actualizar_auto"), false);
  v.enBarraTareas = si(de("barra_tareas"), true);
  v.enBandeja = si(de("bandeja"), true);
  if (!v.modelo.empty() && v.modelo[0] == '~') {
    const char *home = std::getenv("HOME");
    v.modelo = std::string(home ? home : "") + v.modelo.substr(1);
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
const char *const kUrlRedAbierta = "https://huggingface.co/XnetShadow/depth_anything_v2_small/resolve/main/depth_anything_v2_small.onnx";

bool redAbiertaPresente() {
  struct stat sb {};
  return ::stat(rutaRedNuestra().c_str(), &sb) == 0 && sb.st_size == kBytesRedAbierta;
}

namespace {
struct Descarga {
  pid_t pid = 0;            /* el curl, mientras corre */
  std::string parte;        /* el archivo a medias */
  std::string texto;        /* lo que ensena el panel */
  bool fallo = false;
  long bytes = 0;
} g_descarga;
}  // namespace

bool descargaEnMarcha() { return g_descarga.pid > 0; }
std::string textoDescarga() { return g_descarga.texto; }

bool descargar(std::string *err) {
  if (g_descarga.pid > 0) return true;
  const std::string destino = rutaRedNuestra();
  const std::string dir = destino.substr(0, destino.rfind('/'));
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
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
             kUrlRedAbierta, (char *)nullptr);
    ::_exit(127);
  }
  g_descarga.pid = pid;
  g_descarga.fallo = false;
  g_descarga.bytes = 0;
  g_descarga.texto = "descargando… 0 de 95 MB";
  return true;
}

bool vigilarDescarga() {
  if (g_descarga.pid <= 0) return false;
  int estado = 0;
  const pid_t r = ::waitpid(g_descarga.pid, &estado, WNOHANG);
  if (r == 0) {
    struct stat sb {};
    const long bytes = ::stat(g_descarga.parte.c_str(), &sb) == 0 ? (long)sb.st_size : 0;
    if (bytes == g_descarga.bytes) return false;
    g_descarga.bytes = bytes;
    char t[96];
    std::snprintf(t, sizeof t, "descargando… %ld de 95 MB", bytes / 1000000);
    g_descarga.texto = t;
    return true;
  }
  g_descarga.pid = 0;
  const bool bien = r > 0 && WIFEXITED(estado) && WEXITSTATUS(estado) == 0;
  struct stat sb {};
  const bool entera = ::stat(g_descarga.parte.c_str(), &sb) == 0 && sb.st_size == kBytesRedAbierta;
  if (bien && entera && ::rename(g_descarga.parte.c_str(), rutaRedNuestra().c_str()) == 0) {
    g_descarga.texto = "lista: la red abierta ya está en su carpeta";
    g_descarga.fallo = false;
  } else {
    ::unlink(g_descarga.parte.c_str());
    g_descarga.fallo = true;
    g_descarga.texto = !bien ? (r > 0 && WIFEXITED(estado) && WEXITSTATUS(estado) == 127
                                    ? "falta `curl`: instálalo y vuelve a intentarlo"
                                    : "la descarga falló (¿hay conexión?): vuelve a intentarlo")
                             : "el archivo bajó incompleto: vuelve a intentarlo";
  }
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
/* Cuando falta la red de profundidad (o se esta bajando): que hace falta, para que, de donde se
   baja y el boton que lo hace. Con la red en su sitio no pone nada. */
void filasDeLaRed(Hud::State &st, const Valores &v, std::vector<std::string> &ids) {
  const bool abierta = redAbiertaPresente();
  const bool otra = !v.modelo.empty() && v.modelo != rutaRedNuestra() && fileExists(v.modelo);
  if ((abierta || otra) && !descargaEnMarcha() && textoDescarga().empty()) return;
  auto pon = [&](Hud::ItemPanel it, const std::string &id) {
    st.items.push_back(std::move(it));
    ids.push_back(id);
  };
  Hud::ItemPanel s;
  s.tipo = Hud::ItemPanel::Seccion, s.rotulo = abierta ? "RED DE PROFUNDIDAD" : "FALTA LA RED DE PROFUNDIDAD";
  pon(s, "");
  if (!abierta) {
    Hud::ItemPanel i;
    i.tipo = Hud::ItemPanel::Info;
    i.rotulo = "Sin ella no hay datos de profundidad: el vídeo normal no se puede pasar a 3D (el dividido sí).";
    i.nota = "Depth Anything V2 pequeña, 95 MB, de huggingface.co/XnetShadow/depth_anything_v2_small; queda en ~/.local/share/odysseyvk/modelos.";
    pon(i, "");
  }
  Hud::ItemPanel b;
  b.tipo = Hud::ItemPanel::Boton;
  b.rotulo = abierta ? "Red de profundidad" : "Descargar la red para tener profundidad";
  b.nota = textoDescarga();
  b.valorTexto = descargaEnMarcha() ? "Bajando…" : (abierta ? "Lista" : "Descargar (95 MB)");
  b.valor = !descargaEnMarcha() && !abierta;
  pon(b, "descargar_red");
}
}  // namespace

void llenar(Hud::State &st, const Valores &v, const std::vector<Juego> &juegos, std::vector<std::string> &ids) {
  st.pestanas = {"Ajustes", "Juegos", "Rendimiento", "Programa"};
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
  if (st.pestana == 3) {
    /* Programa (v0.95): la version, la actualizacion y el desinstalador */
    Hud::ItemPanel s;
    s.tipo = Hud::ItemPanel::Seccion, s.rotulo = "VERSIÓN";
    pon(s, "");
    Hud::ItemPanel f;
    f.tipo = Hud::ItemPanel::Fila, f.rotulo = "Instalada", f.valorTexto = std::string("OdysseyVK ") + ODYSSEYVK_VERSION;
    pon(f, "");
    Hud::ItemPanel g;
    g.tipo = Hud::ItemPanel::Fila, g.rotulo = "En GitHub";
    if (comprobacionEnMarcha()) g.valorTexto = "comprobando…";
    else if (act.remota.empty()) g.valorTexto = "sin comprobar todavía";
    else {
      char cuando[64] = {0};
      const time_t t = (time_t)act.hora;
      std::strftime(cuando, sizeof cuando, "%H:%M", std::localtime(&t));
      g.valorTexto = "OdysseyVK " + act.remota + (act.pendiente ? " · hay una versión nueva" : " · estás al día") + " (visto a las " + cuando + ")";
    }
    pon(g, "");
    Hud::ItemPanel b;
    b.tipo = Hud::ItemPanel::Boton;
    b.rotulo = act.pendiente ? "Actualizar a OdysseyVK " + act.remota : "Buscar actualizaciones";
    b.nota = act.pendiente ? "Se baja de GitHub, se compila, se instala (pide tu clave) y el servicio se reinicia; el vídeo o juego en 3D se reabre."
                           : "Mira el repositorio " + std::string(kRepoGitHub).substr(8);
    b.valorTexto = comprobacionEnMarcha() ? "Comprobando…" : (act.pendiente ? "Actualizar" : "Buscar");
    b.valor = !comprobacionEnMarcha();
    pon(b, act.pendiente ? "actualizar" : "buscar_actualizacion");
    Hud::ItemPanel a;
    a.tipo = Hud::ItemPanel::Interruptor, a.rotulo = "Avisar cuando haya una versión nueva";
    a.nota = "Una tarjeta en la orilla de la pantalla con el botón de actualizar; sin esto, solo el botón junto al aspa";
    a.valor = v.avisarActualizaciones;
    pon(a, "actualizaciones");
    Hud::ItemPanel au;
    au.tipo = Hud::ItemPanel::Interruptor, au.rotulo = "Actualizar automáticamente";
    au.nota = "En cuanto haya una versión nueva se instala sola (igual pide tu clave para instalar)";
    au.valor = v.actualizarAuto;
    pon(au, "actualizar_auto");
    /* Otros (v1.02): si la ventana del visor 3D sale en la barra de tareas del panel y en Alt+Tab */
    s.rotulo = "OTROS";
    pon(s, "");
    Hud::ItemPanel bt;
    bt.tipo = Hud::ItemPanel::Interruptor, bt.rotulo = "Mostrar el visor 3D en la barra de tareas";
    bt.nota = "El administrador de tareas del panel de KDE; se aplica al momento";
    bt.valor = v.enBarraTareas;
    pon(bt, "barra_tareas");
    Hud::ItemPanel sw;
    sw.tipo = Hud::ItemPanel::Interruptor, sw.rotulo = "Mostrar OdysseyVK en la bandeja del sistema";
    sw.nota = "El icono junto al reloj y las notificaciones: clic abre estos ajustes, clic central los juegos";
    sw.valor = v.enBandeja;
    pon(sw, "bandeja");
    s.rotulo = "DESINSTALAR";
    pon(s, "");
    Hud::ItemPanel d;
    d.tipo = Hud::ItemPanel::Boton, d.rotulo = "Desinstalar OdysseyVK";
    d.nota = "Quita el programa, el servicio y la red de profundidad (pide tu clave); tu configuración se queda.";
    d.valorTexto = g_confirmarDesinstalar > ::time(nullptr) ? "¿Seguro? Pica otra vez" : "Desinstalar";
    d.valor = true;
    pon(d, "desinstalar");
    return;
  }
  if (st.pestana == 0) {
    /* sin ninguna red de profundidad no hay video normal a 3D: lo primero que se ve es como bajarla */
    filasDeLaRed(st, v, ids);
    Hud::ItemPanel s;
    s.tipo = Hud::ItemPanel::Seccion, s.rotulo = "VÍDEO";
    pon(s, "");
    Hud::ItemPanel m;
    m.tipo = Hud::ItemPanel::Opciones, m.rotulo = "Al ver un vídeo a pantalla completa";
    m.opciones = {"Preguntar", "Vídeo normal", "Vídeo dividido"};
    m.elegido = v.modo == "conv" ? 1 : (v.modo == "sbs" ? 2 : 0);
    pon(m, "modo");
    Hud::ItemPanel d;
    d.tipo = Hud::ItemPanel::Opciones, d.rotulo = "Suavizar los vídeos divididos";
    d.opciones = {"Siempre", "Si hace falta", "Nunca"};
    d.elegido = v.suavizar == 1 ? 0 : (v.suavizar == 2 ? 1 : 2);
    pon(d, "dividido_suavizado");
    Hud::ItemPanel r;
    r.tipo = Hud::ItemPanel::Deslizador, r.rotulo = "Fuerza del relieve";
    r.nota = "Para el vídeo normal y el dividido suavizado · desde el siguiente vídeo";
    r.frac = (float)((v.relieve - 0.5) / 1.5), r.valorTexto = conComa(v.relieve, 2);
    pon(r, "relieve");

    s.rotulo = "PANTALLA";
    pon(s, "");
    Hud::ItemPanel f;
    f.tipo = Hud::ItemPanel::Deslizador, f.rotulo = "Reducir el fantasma";
    f.nota = "Cuánta imagen del otro ojo se resta · se nota al momento";
    f.frac = (float)(v.fantasma / 0.10), f.valorTexto = conComa(v.fantasma * 100.0, 1) + " %";
    pon(f, "crosstalk_base");
    Hud::ItemPanel c;
    c.tipo = Hud::ItemPanel::Interruptor, c.rotulo = "Seguir la cabeza con la cámara";
    c.nota = "Desde el siguiente vídeo", c.valor = v.rastreo;
    pon(c, "rastreo");
    Hud::ItemPanel h;
    h.tipo = Hud::ItemPanel::Interruptor, h.rotulo = "Mantener el HDR durante el 3D";
    h.nota = "Desde el siguiente vídeo", h.valor = v.hdr;
    pon(h, "hdr_en_3d");
    Hud::ItemPanel a;
    a.tipo = Hud::ItemPanel::Interruptor, a.rotulo = "Atajos de teclado en todo el escritorio";
    a.nota = "Ctrl+Shift+1…8 y F1 valen aunque el teclado lo tenga el vídeo o el juego", a.valor = v.atajos;
    pon(a, "atajos_globales");

    s.rotulo = "APLICACIONES QUE NO PREGUNTAN";
    pon(s, "");
    if (v.recordadas.empty()) {
      Hud::ItemPanel n;
      n.tipo = Hud::ItemPanel::Nota, n.rotulo = "Ninguna: se pregunta siempre (o se sigue lo de arriba).";
      pon(n, "");
    }
    for (const auto &par : v.recordadas) {
      if (par.first.rfind("steam_app_", 0) == 0) continue;   /* los juegos se llevan en su pestana */
      Hud::ItemPanel fila;
      fila.tipo = Hud::ItemPanel::Fila, fila.rotulo = par.first, fila.valorTexto = nombreModo(par.second);
      fila.quitar = true;
      pon(fila, "auto:" + par.first);
    }
  } else if (st.pestana == 2) {
    /* Rendimiento (v0.86): lo que el visor cuenta en su `estado` (dos veces por segundo) y lo que
       dice nvidia-smi de las tarjetas. Se rellena de nuevo cada segundo mientras esta a la vista. */
    auto fila = [&](const std::string &que, const std::string &cuanto) {
      Hud::ItemPanel f;
      f.tipo = Hud::ItemPanel::Fila, f.rotulo = que, f.valorTexto = cuanto;
      pon(f, "");
    };
    Hud::ItemPanel s;
    s.tipo = Hud::ItemPanel::Seccion, s.rotulo = "VISOR 3D";
    pon(s, "");
    const auto hub = ajuste::leerEstadoHub();
    const auto ses = hub.find("sesion");
    const bool abierta = ses != hub.end() && !ses->second.empty() && ses->second != "ninguna";
    if (!abierta) {
      Hud::ItemPanel n;
      n.tipo = Hud::ItemPanel::Nota, n.rotulo = "No hay ningún vídeo ni juego en 3D ahora mismo.";
      pon(n, "");
    } else {
      const auto e = ajuste::leerEstado();
      auto de = [&](const char *k) {
        const auto it = e.find(k);
        return it == e.end() ? std::string("-") : it->second;
      };
      std::string fps = de("fps"), ms = de("mapa_ms"), modelo = de("modelo");
      for (char &c : fps) if (c == '.') c = ',';
      for (char &c : ms) if (c == '.') c = ',';
      const size_t barra = modelo.rfind('/');
      if (barra != std::string::npos) modelo = modelo.substr(barra + 1);
      fila("Cuadros por segundo", fps);
      const bool conv = de("conversor") == "puesto";
      fila("Conversor de relieve", conv ? "puesto · " + ms + " ms por mapa" : "apagado");
      if (conv) fila("Red de profundidad", modelo + " · " + de("tarjeta"));
      fila("Cámara", de("facetracking") == "abierto" ? de("camara_nota") : "sin abrir");
    }
    filasDeLaRed(st, v, ids);
    s.rotulo = "TARJETAS GRÁFICAS";
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
        std::snprintf(txt, sizeof txt, "%s %% · %.1f de %.0f GB · %.0f °C", c[1].c_str(), std::atof(c[2].c_str()) / 1024.0,
                      std::atof(c[3].c_str()) / 1024.0, std::atof(c[4].c_str()));
        std::string t = txt;
        for (char &ch : t) if (ch == '.') ch = ',';
        fila(nombre, t);
        alguna = true;
      }
      ::pclose(f);
    }
    if (!alguna) {
      Hud::ItemPanel n;
      n.tipo = Hud::ItemPanel::Nota, n.rotulo = "nvidia-smi no respondió: no hay datos de las tarjetas.";
      pon(n, "");
    }
  } else {
    Hud::ItemPanel s;
    s.tipo = Hud::ItemPanel::Seccion, s.rotulo = "STEAM";
    pon(s, "");
    if (juegos.empty()) {
      Hud::ItemPanel n;
      n.tipo = Hud::ItemPanel::Nota, n.rotulo = "No encontré juegos instalados en las bibliotecas de Steam.";
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
      pon(fila, "juego:" + j.id);
    }
    Hud::ItemPanel n;
    n.tipo = Hud::ItemPanel::Nota;
    n.rotulo = "3D: el juego entra solo en 3D al ponerse a pantalla completa. 2D: se deja como está.";
    pon(n, "");
  }
}

bool aplicar(const std::string &id, int sub, float frac, bool soltado, const std::vector<Juego> &juegos,
             std::string *aviso, bool *cerrar) {
  std::string e;
  auto fija = [&](const std::string &k, const std::string &val) {
    if (ajuste::fijarEnServeConf(k, val, &e)) return true;
    if (aviso) *aviso = "no se pudo guardar: " + e;
    return false;
  };
  const Valores v = leer();
  if (id == "modo") {
    static const char *kModo[] = {"ask", "conv", "sbs"};
    if (sub < 0 || sub > 2 || !fija("modo", kModo[sub])) return false;
    if (aviso) *aviso = sub == 0 ? "se preguntará con la tarjeta" : "desde el siguiente vídeo, sin preguntar";
    return true;
  }
  if (id == "dividido_suavizado") {
    static const char *kVal[] = {"si", "auto", "no"};
    static const char *kDice[] = {"los vídeos divididos se suavizan siempre",
                                  "con su propio relieve mientras quepa; suavizado solo si algo sale doble",
                                  "los vídeos divididos se ven siempre con su propio relieve"};
    if (sub < 0 || sub > 2 || !fija(id, kVal[sub])) return false;
    avisarVisor({}, {{"dividido_suavizado", kVal[sub]}});
    if (aviso) *aviso = kDice[sub];
    return true;
  }
  if (id == "rastreo" || id == "hdr_en_3d" || id == "atajos_globales") {
    const bool ahora = id == "rastreo" ? v.rastreo : (id == "hdr_en_3d" ? v.hdr : v.atajos);
    if (!fija(id, ahora ? "no" : "si")) return false;
    if (aviso) *aviso = "se aplica desde el siguiente vídeo";
    return true;
  }
  if (id == "crosstalk_base") {
    const double val = std::round(std::max(0.0f, std::min(1.0f, frac)) * 0.10 / 0.002) * 0.002;
    avisarVisor({{"crosstalk_base", val}}, {});   /* se ve al momento, tambien mientras se arrastra */
    if (!soltado) return false;
    if (!fija(id, conComa(val, 4))) return false;
    if (aviso) *aviso = "se resta el " + conComa(val * 100.0, 1) + " % de la imagen del otro ojo";
    return true;
  }
  if (id == "relieve") {
    if (!soltado) return false;
    const double val = std::round((0.5 + std::max(0.0f, std::min(1.0f, frac)) * 1.5) / 0.05) * 0.05;
    if (!fija(id, conComa(val, 2))) return false;
    if (aviso) *aviso = "fuerza del relieve " + conComa(val, 2) + " · desde el siguiente vídeo";
    return true;
  }
  if (id == "actualizaciones") {
    if (!fija("actualizaciones", v.avisarActualizaciones ? "no" : "si")) return false;
    if (aviso) *aviso = v.avisarActualizaciones ? "no se avisará de versiones nuevas; el botón junto al aspa las sigue marcando"
                                                : "se avisará con un aviso del escritorio";
    return true;
  }
  if (id == "barra_tareas" || id == "bandeja") {
    const bool ahora = id == "barra_tareas" ? v.enBarraTareas : v.enBandeja;
    if (!fija(id, ahora ? "no" : "si")) return false;
    if (aviso) *aviso = id == "barra_tareas" ? std::string(ahora ? "el visor 3D ya no sale en la barra de tareas" : "el visor 3D vuelve a salir en la barra de tareas")
                                             : std::string(ahora ? "sin icono en la bandeja del sistema" : "con icono en la bandeja del sistema");
    return true;
  }
  if (id == "actualizar_auto") {
    if (!fija("actualizar_auto", v.actualizarAuto ? "no" : "si")) return false;
    if (aviso) *aviso = v.actualizarAuto ? "las versiones nuevas ya no se instalan solas" : "las versiones nuevas se instalarán solas";
    return true;
  }
  if (id == "buscar_actualizacion") {
    if (!comprobarActualizacion(&e)) {
      if (aviso) *aviso = e;
      return false;
    }
    if (aviso) *aviso = "mirando la versión de GitHub…";
    return true;
  }
  if (id == "actualizar") {
    if (!lanzarActualizacion(&e)) {
      if (aviso) *aviso = e;
      return false;
    }
    if (aviso) *aviso = "actualizando: se compila, se pide tu clave para instalar y el servicio se reinicia";
    if (cerrar) *cerrar = true;
    return true;
  }
  if (id == "desinstalar") {
    const long ahora = (long)::time(nullptr);
    if (g_confirmarDesinstalar <= ahora) {
      g_confirmarDesinstalar = ahora + 12;
      if (aviso) *aviso = "¿seguro? pica otra vez en los próximos 12 segundos para desinstalar";
      return true;
    }
    g_confirmarDesinstalar = 0;
    if (!lanzarDesinstalacion(&e)) {
      if (aviso) *aviso = e;
      return false;
    }
    if (aviso) *aviso = "desinstalando OdysseyVK…";
    if (cerrar) *cerrar = true;
    return true;
  }
  if (id == "descargar_red") {
    if (!descargar(&e)) {
      if (aviso) *aviso = e;
      return false;
    }
    if (aviso) *aviso = "bajando la red abierta (95 MB)…";
    return true;
  }
  if (id.rfind("auto:", 0) == 0) {
    if (!ajuste::fijarAutoEnServeConf(id.substr(5), std::string(), &e)) {
      if (aviso) *aviso = "no se pudo guardar: " + e;
      return false;
    }
    if (aviso) *aviso = "se volverá a preguntar por " + id.substr(5);
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
        if (aviso) *aviso = "no se pudo guardar: " + e;
        return false;
      }
      if (aviso) *aviso = en3d ? "ese juego se queda en 2D" : "ese juego entra solo en 3D";
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
    if (aviso) *aviso = "arrancando " + nombre + "…";
    if (cerrar) *cerrar = true;
    return true;
  }
  return false;
}

}  /* namespace prefs */
