#include "util.h"

#include <array>
#include <algorithm>
#include <map>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <sys/wait.h>
#include <ctime>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

std::string strTrim(const std::string &s) {
  size_t a = 0, b = s.size();
  while (a < b && (unsigned char)s[a] <= ' ') a++;
  while (b > a && (unsigned char)s[b - 1] <= ' ') b--;
  return s.substr(a, b - a);
}

std::string strLower(std::string s) {
  for (char &c : s)
    if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
  return s;
}

static int hexVal(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

std::vector<uint8_t> unhex(const std::string &s) {
  std::vector<uint8_t> out;
  int hi = -1;
  for (char c : s) {
    int v = hexVal(c);
    if (v < 0) continue; /* ignora separadores */
    if (hi < 0) {
      hi = v;
    } else {
      out.push_back((uint8_t)((hi << 4) | v));
      hi = -1;
    }
  }
  return out;
}

std::string toHexUpper(const uint8_t *p, size_t n) {
  static const char *D = "0123456789ABCDEF";
  std::string s;
  s.reserve(2 * n);
  for (size_t i = 0; i < n; i++) {
    s.push_back(D[p[i] >> 4]);
    s.push_back(D[p[i] & 15]);
  }
  return s;
}

std::string toHexUpper(const std::vector<uint8_t> &v) {
  return toHexUpper(v.data(), v.size());
}

bool readSmallFile(const std::string &path, std::string &out) {
  std::ifstream f(path, std::ios::binary);
  if (!f) return false;
  std::ostringstream ss;
  ss << f.rdbuf();
  out = ss.str();
  return true;
}

/* ------ del nombre viejo (odyssey3d) al nuevo (odysseyvk) ------ */

/* Copia `de` dentro de `a` sin pisar lo que ya esta. Solo archivos: los directorios se crean. */
static int copiarArbol(const std::filesystem::path &de, const std::filesystem::path &a) {
  std::error_code ec;
  if (!std::filesystem::is_directory(de, ec)) return 0;
  int n = 0;
  for (std::filesystem::recursive_directory_iterator it(de, std::filesystem::directory_options::skip_permission_denied, ec), fin;
       it != fin; it.increment(ec)) {
    if (ec) break;
    const std::filesystem::path &orig = it->path();
    if (!it->is_regular_file(ec) || ec) continue;
    std::filesystem::path rel = std::filesystem::relative(orig, de, ec);
    if (ec) continue;
    /* (v1.16) la carpeta de las redes se llama models: lo del nombre viejo va ahi, no a una modelos nueva */
    if (!rel.empty() && *rel.begin() == "modelos") rel = std::filesystem::path("models") / std::filesystem::relative(rel, "modelos");
    std::filesystem::path destino = a / rel;
    if (std::filesystem::exists(destino, ec)) continue;   /* lo nuevo manda: no se pisa nada */
    std::filesystem::create_directories(destino.parent_path(), ec);
    std::filesystem::copy_file(orig, destino, std::filesystem::copy_options::skip_existing, ec);
    if (!ec) n++;
  }
  return n;
}

/* (v1.16) ~/.local/share/odysseyvk/modelos pasa a llamarse models (Rodolfo: «en esta update actualiza el nombre de
   esa carpeta para todos los usuarios»): se mueve entera (o, si ya estaban las dos, lo que falte en la nueva), y las
   rutas que la nombran en serve.conf, en el ajuste vivo y en los guardados por red pasan a la nueva. Cuesta una
   mirada a una carpeta en cada arranque; mover es al momento (el mismo disco) */
static int migrarCarpetaDeRedes(const std::string &h) {
  namespace fs = std::filesystem;
  std::error_code ec;
  const fs::path base = fs::path(h) / ".local/share/odysseyvk", viejo = base / "modelos", nuevo = base / "models";
  if (fs::is_symlink(viejo, ec) || !fs::is_directory(viejo, ec)) return 0;
  int n = 0;
  if (!fs::exists(nuevo, ec)) {
    fs::rename(viejo, nuevo, ec);
    if (ec) {
      std::printf("[redes] no se pudo cambiar el nombre de %s a models: %s\n", viejo.c_str(), ec.message().c_str());
      return 0;
    }
    n = 1;
  } else {
    for (const fs::directory_entry &e : fs::directory_iterator(viejo, ec)) {
      std::error_code e2;
      const fs::path d = nuevo / e.path().filename();
      if (fs::exists(d, e2)) continue;   /* lo de la nueva manda */
      fs::rename(e.path(), d, e2);
      if (!e2) n++;
    }
    fs::remove(viejo, ec);   /* solo si quedo vacia */
  }
  int rutas = 0;
  const std::string a = "/odysseyvk/modelos/", b = "/odysseyvk/models/";
  for (const std::string &f : {h + "/.config/odysseyvk/serve.conf", rutaEstadoOdyssey("ajuste"), rutaAjusteOdyssey("perfiles")}) {
    std::string t;
    if (!readSmallFile(f, t) || t.find(a) == std::string::npos) continue;
    for (size_t p = t.find(a); p != std::string::npos; p = t.find(a, p + b.size())) t.replace(p, a.size(), b), rutas++;
    const std::string tmp = f + ".tmp";
    if (FILE *o = std::fopen(tmp.c_str(), "we")) {
      std::fputs(t.c_str(), o);
      std::fclose(o);
      std::error_code e3;
      fs::rename(tmp, f, e3);
    }
  }
  std::printf("[redes] la carpeta de las redes ahora es %s (%d movido%s, %d ruta%s al dia)\n", nuevo.c_str(), n, n == 1 ? "" : "s",
              rutas, rutas == 1 ? "" : "s");
  return n;
}

int migrarDatosDeViejoNombre() {
  static int hecho = -1;
  if (hecho >= 0) return hecho;
  const char *home = std::getenv("HOME");
  const std::string h = home ? home : "/home";
  const char *xdgState = std::getenv("XDG_STATE_HOME");
  struct Pareja { std::string viejo, nuevo; };
  const Pareja carpetas[] = {
      {h + "/.config/odyssey3d", h + "/.config/odysseyvk"},
      {(xdgState && *xdgState ? std::string(xdgState) + "/odyssey3d" : h + "/.local/state/odyssey3d"),
       (xdgState && *xdgState ? std::string(xdgState) + "/odysseyvk" : h + "/.local/state/odysseyvk")},
      {h + "/.local/share/odyssey3d", h + "/.local/share/odysseyvk"},
  };
  int tot = 0;
  for (const Pareja &c : carpetas) {
    std::error_code ec;
    if (!std::filesystem::is_directory(c.viejo, ec)) continue;
    const int n = copiarArbol(c.viejo, c.nuevo);
    if (n > 0) {
      std::printf("[nombre] copiados %d archivos de %s a %s (lo del nombre viejo se deja intacto)\n", n,
                  c.viejo.c_str(), c.nuevo.c_str());
      tot += n;
    }
  }
  migrarCarpetaDeRedes(h);   /* despues: lo copiado del nombre viejo ya va a models */
  hecho = tot;
  return hecho;
}

const char *entornoOdyssey(const char *nombre) {
  if (!nombre || !*nombre) return nullptr;
  if (const char *v = std::getenv(nombre)) return v;
  /* el nombre viejo es el mismo con ODYSSEY3D_ en lugar de ODYSSEYVK_ */
  static const std::string pref = "ODYSSEYVK_", viejo = "ODYSSEY3D_";
  if (std::string(nombre).compare(0, pref.size(), pref) != 0) return nullptr;
  static std::string altern;   /* getenv necesita que la cadena siga viva */
  altern = viejo + (nombre + pref.size());
  return std::getenv(altern.c_str());
}

std::string rutaConfigOdyssey(const char *nombre) {
  const char *home = std::getenv("HOME");
  return std::string(home ? home : "/home") + "/.config/odysseyvk/" + nombre;
}

std::string rutaAjusteOdyssey(const char *nombre) {
  const std::string nueva = rutaConfigOdyssey(nombre);
  std::error_code ec;
  if (!std::filesystem::exists(nueva, ec)) {
    const std::string vieja = rutaEstadoOdyssey(nombre);
    if (std::filesystem::exists(vieja, ec)) {
      std::filesystem::create_directories(std::filesystem::path(nueva).parent_path(), ec);
      std::filesystem::rename(vieja, nueva, ec);
      if (ec) {   /* otro sistema de archivos: se copia y se borra la vieja */
        std::error_code ec2;
        if (std::filesystem::copy_file(vieja, nueva, ec2)) std::filesystem::remove(vieja, ec2);
      }
    }
  }
  return nueva;
}

std::string rutaEstadoOdyssey(const char *nombre) {
  const char *xdg = std::getenv("XDG_STATE_HOME");
  std::string dir;
  if (xdg && *xdg)
    dir = std::string(xdg) + "/odysseyvk";
  else {
    const char *home = std::getenv("HOME");
    dir = std::string(home ? home : "/home") + "/.local/state/odysseyvk";
  }
  std::error_code ec;
  std::filesystem::create_directories(std::filesystem::path(dir), ec);
  return dir + "/" + nombre;
}

/* la red abierta: desde la rama experimental (26-nov) la de fabrica es la fp16 (la misma red a media
   precision: el mismo mapa con un 35 % menos de tiempo y 260 MiB menos, medido en la RTX 5080). Si
   solo esta la de siempre (fp32), se usa esa hasta que se baje la ligera. */
std::string carpetaRedes() {
  const char *home = std::getenv("HOME");
  return std::string(home ? home : "/home") + "/.local/share/odysseyvk/models/";   /* (v1.16) antes modelos */
}
std::string rutaRedLigera() { return carpetaRedes() + "depth_anything_v2_small_fp16.onnx"; }
std::string rutaRedVieja() { return carpetaRedes() + "depth_anything_v2_small.onnx"; }

bool esRedDeVideo(const std::string &ruta) {
  const size_t p = ruta.find_last_of('/');
  return strLower(p == std::string::npos ? ruta : ruta.substr(p + 1)).find("video") != std::string::npos;
}
std::string rutaRedVideo() { return carpetaRedes() + "video_depth_anything_small_fp32.onnx"; }

/* (v1.16) ver util.h. Solo los cardN (no sus conectores, cardN-DP-1) del fabricante 0x1002 con amdgpu */
std::vector<TarjetaAmd> tarjetasAmd() {
  std::vector<TarjetaAmd> v;
  namespace fs = std::filesystem;
  std::error_code ec;
  std::vector<std::string> cards;
  for (const fs::directory_entry &e : fs::directory_iterator("/sys/class/drm", ec)) {
    const std::string n = e.path().filename().string();
    if (n.rfind("card", 0) == 0 && n.size() > 4 && n.find('-') == std::string::npos) cards.push_back(e.path().string());
  }
  std::sort(cards.begin(), cards.end());
  auto lee = [](const std::string &ruta) {
    std::string t;
    return readSmallFile(ruta, t) ? strTrim(t) : std::string();
  };
  for (const std::string &c : cards) {
    const std::string dev = c + "/device";
    if (strLower(lee(dev + "/vendor")) != "0x1002") continue;
    std::error_code e2;
    if (fs::path(fs::read_symlink(dev + "/driver", e2)).filename() != "amdgpu") continue;
    TarjetaAmd t;
    t.pci = strLower(fs::path(fs::canonical(dev, e2)).filename().string());
    const std::string total = lee(dev + "/mem_info_vram_total"), usada = lee(dev + "/mem_info_vram_used");
    if (!total.empty()) t.totalMiB = std::atol(total.c_str()) / (1024L * 1024L);
    if (!usada.empty()) t.usadaMiB = std::atol(usada.c_str()) / (1024L * 1024L);
    const std::string ocupada = lee(dev + "/gpu_busy_percent");
    if (!ocupada.empty()) t.ocupada = std::atoi(ocupada.c_str());
    for (const fs::directory_entry &h : fs::directory_iterator(dev + "/hwmon", e2)) {
      const std::string temp = lee(h.path().string() + "/temp1_input");
      if (!temp.empty()) {
        t.tempC = std::atof(temp.c_str()) / 1000.0;
        break;
      }
    }
    t.nombre = lee(dev + "/product_name");
    /* (v1.17) el nombre sacado de pci.ids, guardado: el monitor del cuadrito lee las tarjetas cada segundo */
    static std::map<std::string, std::string> nombres;
    if (t.nombre.empty())
      if (const auto it = nombres.find(t.pci); it != nombres.end()) t.nombre = it->second;
    if (t.nombre.empty()) {
      /* pci.ids: «\t13c0  Granite Ridge [Radeon 610M]» bajo «1002  Advanced Micro Devices...»: lo de los corchetes */
      std::string id = strLower(lee(dev + "/device"));
      if (id.rfind("0x", 0) == 0) id = id.substr(2);
      std::ifstream ids("/usr/share/hwdata/pci.ids");
      std::string l;
      bool enAmd = false;
      while (std::getline(ids, l)) {
        if (!l.empty() && l[0] != '\t' && l[0] != '#') {
          if (enAmd) break;
          enAmd = l.rfind("1002 ", 0) == 0;
        } else if (enAmd && l.size() > 7 && l[0] == '\t' && l[1] != '\t' && strLower(l.substr(1, 4)) == id) {
          std::string nom = strTrim(l.substr(5));
          const size_t a = nom.find('['), b = nom.rfind(']');
          t.nombre = a != std::string::npos && b != std::string::npos && b > a + 1 ? nom.substr(a + 1, b - a - 1) : nom;
          break;
        }
      }
      if (t.nombre.empty()) t.nombre = "AMD " + lee(dev + "/device");
      nombres[t.pci] = t.nombre;
    }
    v.push_back(t);
  }
  return v;
}

std::string pciDeConector(const std::string &conector) {
  namespace fs = std::filesystem;
  if (conector.empty()) return {};
  std::error_code ec;
  for (const fs::directory_entry &c : fs::directory_iterator("/sys/class/drm", ec)) {
    const std::string n = c.path().filename().string();   /* «card2-DP-4» */
    const size_t g = n.find('-');
    if (n.rfind("card", 0) != 0 || g == std::string::npos || n.substr(g + 1) != conector) continue;
    std::error_code e2;
    const std::string pci = fs::canonical(fs::path("/sys/class/drm") / n.substr(0, g) / "device", e2).filename().string();
    if (!e2) return pci;
  }
  return {};
}

std::string conectorConEdid(const std::string &texto) {
  namespace fs = std::filesystem;
  std::error_code ec;
  for (const fs::directory_entry &c : fs::directory_iterator("/sys/class/drm", ec)) {
    const std::string n = c.path().filename().string();
    const size_t g = n.find('-');
    if (n.rfind("card", 0) != 0 || g == std::string::npos) continue;
    std::string st, edid;
    if (!readSmallFile(c.path().string() + "/status", st) || strTrim(st) != "connected") continue;
    if (readSmallFile(c.path().string() + "/edid", edid) && edid.find(texto) != std::string::npos) return n.substr(g + 1);
  }
  return {};
}

std::string nombrePciIds(const std::string &fabricante, const std::string &modelo) {
  auto limpio = [](std::string s) {
    s = strLower(strTrim(s));
    return s.rfind("0x", 0) == 0 ? s.substr(2) : s;
  };
  const std::string fab = limpio(fabricante), mod = limpio(modelo);
  if (fab.size() != 4 || mod.size() != 4) return {};
  std::ifstream ids("/usr/share/hwdata/pci.ids");
  std::string l;
  bool enFab = false;
  while (std::getline(ids, l)) {
    if (!l.empty() && l[0] != '\t' && l[0] != '#') {
      if (enFab) break;
      enFab = strLower(l.substr(0, 4)) == fab && l.size() > 4 && l[4] == ' ';
    } else if (enFab && l.size() > 7 && l[0] == '\t' && l[1] != '\t' && strLower(l.substr(1, 4)) == mod) {
      const std::string nom = strTrim(l.substr(5));
      const size_t a = nom.find('['), b = nom.rfind(']');
      return a != std::string::npos && b != std::string::npos && b > a + 1 ? nom.substr(a + 1, b - a - 1) : nom;
    }
  }
  return {};
}

namespace {
/* segundos desde el arranque del sistema en que empezo este proceso (campo 22 de /proc/self/stat,
   en tics de reloj); -1 si no se pudo leer */
double inicioProceso() {
  static const double inicio = [] {
    std::ifstream f("/proc/self/stat");
    std::string todo((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    const size_t p = todo.rfind(')');
    if (p == std::string::npos) return -1.0;
    std::istringstream rest(todo.substr(p + 2));
    std::string campo;
    for (int i = 3; i <= 22 && (rest >> campo); i++) {
      if (i == 22) return std::atof(campo.c_str()) / (double)sysconf(_SC_CLK_TCK);
    }
    return -1.0;
  }();
  return inicio;
}
std::vector<std::pair<std::string, double>> &marcas() {
  static std::vector<std::pair<std::string, double>> m;
  return m;
}
}  // namespace

std::mutex &cerrojoMarcas() {
  static std::mutex m;
  return m;
}

void marcaArranque(const char *paso) {
  timespec ts{};
  clock_gettime(CLOCK_BOOTTIME, &ts);
  const double ahora = (double)ts.tv_sec + 1e-9 * (double)ts.tv_nsec, ini = inicioProceso();
  std::lock_guard<std::mutex> g(cerrojoMarcas());   /* la lente y el scaler marcan desde su hilo (v1.12) */
  marcas().push_back({paso, ini >= 0.0 ? (ahora - ini) * 1000.0 : -1.0});
}

/* (v1.12) con pasos en paralelo, las restas entre marcas seguidas ya no dicen cuanto tardo cada uno: el
   resumen da el MOMENTO en que acabo cada paso, en ms desde que arranco el proceso */
std::string resumenArranque() {
  std::lock_guard<std::mutex> g(cerrojoMarcas());
  std::string s;
  double ultimo = 0.0;
  char buf[96];
  for (const auto &m : marcas()) {
    std::snprintf(buf, sizeof buf, "%s%s %.0f", s.empty() ? "" : " · ", m.first.c_str(), m.second);
    s += buf;
    ultimo = std::max(ultimo, m.second);
  }
  return s + " (ms desde que arranca el proceso)";
}
std::string rutaRedNuestra() {
  if (fileExists(rutaRedLigera())) return rutaRedLigera();
  if (fileExists(rutaRedVieja())) return rutaRedVieja();
  return rutaRedLigera();
}

int slotRed(const std::string &ruta) {
  const std::string s = strLower(ruta);
  return (s.find("mde_") != std::string::npos || s.find("disp_") != std::string::npos) ? 1 : 2;
}

int leerRedPreferida() {
  std::string txt;
  if (!readSmallFile(rutaConfigOdyssey("red-elegida"), txt)) return 0;
  txt = strTrim(txt);
  return txt == "1" ? 1 : (txt == "2" ? 2 : 0);
}

void grabarRedPreferida(int red) {
  if (red != 1 && red != 2) return;
  const std::string ruta = rutaConfigOdyssey("red-elegida");
  if (FILE *f = std::fopen(ruta.c_str(), "we")) {   /* 'e': sin permiso de ejecucion */
    std::fprintf(f, "%d\n", red);
    std::fclose(f);
  }
}

bool fileExists(const std::string &path) {
  return !path.empty() && access(path.c_str(), R_OK) == 0;
}

bool readBinaryFile(const std::string &path, std::vector<uint8_t> &out) {
  out.clear();
  std::ifstream f(path, std::ios::binary);
  if (!f) return false;
  f.seekg(0, std::ios::end);
  const std::streamoff n = f.tellg();
  if (n <= 0 || n > (std::streamoff)64 * 1024 * 1024) return false;
  f.seekg(0, std::ios::beg);
  out.resize((size_t)n);
  f.read((char *)out.data(), n);
  if (!f) {
    out.clear();
    return false;
  }
  return true;
}

int runCapture(const std::string &cmd, std::string &out) {
  std::array<char, 4096> buf{};
  FILE *p = popen(cmd.c_str(), "r");
  if (!p) return -1;
  while (fgets(buf.data(), (int)buf.size(), p)) out += buf.data();
  int rc = pclose(p);
  return WIFEXITED(rc) ? WEXITSTATUS(rc) : -1;
}

std::string usbIdToHex(uint16_t v) {
  char b[8];
  snprintf(b, sizeof b, "%04x", v);
  return b;
}

#include <cstdlib>
#include <filesystem>
#include <unistd.h>

void execPanelDeAjustes(const std::vector<std::string> &args) {
  char propio[4096] = {0};
  const ssize_t n = ::readlink("/proc/self/exe", propio, sizeof propio - 1);
  const std::string exe = n > 0 ? std::string(propio, (size_t)n) : std::string("odysseyvk");
  const std::string ligero = exe.substr(0, exe.rfind('/') + 1) + "odysseyvk-panel";
  const char *pesado = std::getenv("ODYSSEYVK_PANEL_PESADO");
  std::vector<std::string> v;
  if (!(pesado && pesado[0] == '1') && ::access(ligero.c_str(), X_OK) == 0) {
    v.push_back(ligero);
  } else {
    v.push_back(exe);
    v.push_back("ajustes");
  }
  v.insert(v.end(), args.begin(), args.end());
  std::vector<char *> argv;
  for (std::string &a : v) argv.push_back(a.data());
  argv.push_back(nullptr);
  ::execv(argv[0], argv.data());
  if (v[0] == ligero) {   /* el ligero no arranco: el de siempre */
    std::vector<std::string> w = {exe, "ajustes"};
    w.insert(w.end(), args.begin(), args.end());
    std::vector<char *> argw;
    for (std::string &a : w) argw.push_back(a.data());
    argw.push_back(nullptr);
    ::execv(argw[0], argw.data());
  }
}

std::string ensureWaylandEnv() {
  const char *wd = getenv("WAYLAND_DISPLAY");
  if (wd && *wd) return wd;
  const char *xdg = getenv("XDG_RUNTIME_DIR");
  std::string dir = (xdg && *xdg) ? xdg : ("/run/user/" + std::to_string((long long)getuid()));
  std::error_code ec;
  if (!std::filesystem::is_directory(dir, ec)) return {};
  std::vector<std::string> found;
  for (const auto &e : std::filesystem::directory_iterator(dir, ec)) {
    std::string n = e.path().filename().string();
    if (n.rfind("wayland-", 0) == 0 && std::filesystem::is_socket(e.path(), ec)) found.push_back(n);
  }
  if (found.empty()) return {};
  std::sort(found.begin(), found.end());
  setenv("WAYLAND_DISPLAY", found[0].c_str(), 1);
  setenv("XDG_RUNTIME_DIR", dir.c_str(), 1);
  return found[0];
}

/* el panel fijado encima (rama experimental): lo apunta el pin del panel y lo mira el visor, que con
   el panel fijado no lo cierra cuando se pica el juego */
bool panelFijado() {
  std::string t;
  return readSmallFile(rutaAjusteOdyssey("panel-fijado"), t) && strTrim(t) == "si";
}
void fijarPanel(bool si) {
  if (FILE *f = std::fopen(rutaAjusteOdyssey("panel-fijado").c_str(), "we")) {
    std::fputs(si ? "si\n" : "no\n", f);
    std::fclose(f);
  }
}

/* el color de resaltado de KDE (rama experimental), de ~/.config/kdeglobals: `[General] AccentColor` si
   esta (Plasma 6 con un acento propio) o `[Colors:Selection] BackgroundNormal` (el del esquema de color;
   en Breeze, 61,174,233). Devuelve false si no hay KDE o no se entiende. */
bool colorResaltadoKde(float rgb[3]) {
  const char *xdg = std::getenv("XDG_CONFIG_HOME");
  const char *home = std::getenv("HOME");
  const std::string ruta = (xdg && *xdg) ? std::string(xdg) + "/kdeglobals" : std::string(home ? home : "") + "/.config/kdeglobals";
  std::string txt;
  if (!readSmallFile(ruta, txt)) return false;
  std::string seccion, acento, seleccion;
  size_t i = 0;
  while (i < txt.size()) {
    size_t f = txt.find('\n', i);
    if (f == std::string::npos) f = txt.size();
    const std::string l = strTrim(txt.substr(i, f - i));
    i = f + 1;
    if (!l.empty() && l[0] == '[') { seccion = l; continue; }
    if (seccion == "[General]" && l.rfind("AccentColor=", 0) == 0) acento = l.substr(12);
    if (seccion == "[Colors:Selection]" && l.rfind("BackgroundNormal=", 0) == 0) seleccion = l.substr(17);
  }
  const std::string &v = !acento.empty() ? acento : seleccion;
  int r = 0, g = 0, b = 0;
  if (v.empty() || std::sscanf(v.c_str(), "%d,%d,%d", &r, &g, &b) != 3) return false;
  rgb[0] = (float)r / 255.0f, rgb[1] = (float)g / 255.0f, rgb[2] = (float)b / 255.0f;
  return true;
}
