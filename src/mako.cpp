/* (v1.18) la generacion de cuadros de MAKO: su conf.toml (ver mako.h) */
#include "mako.h"

#include "idioma.h"
#include "util.h"

#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace mako {
namespace {

/* una linea sin su comentario (un # fuera de comillas) y sin espacios a los lados */
std::string sinComentario(const std::string &l) {
  char comilla = 0;
  for (size_t i = 0; i < l.size(); i++) {
    const char c = l[i];
    if (comilla) {
      if (c == comilla) comilla = 0;
    } else if (c == '\'' || c == '"') {
      comilla = c;
    } else if (c == '#') {
      return strTrim(l.substr(0, i));
    }
  }
  return strTrim(l);
}

/* la clave de una linea «clave = valor» (vacio si no es eso) y su valor */
bool parClave(const std::string &linea, std::string &clave, std::string &valor) {
  const std::string l = sinComentario(linea);
  const size_t ig = l.find('=');
  if (l.empty() || l[0] == '[' || ig == std::string::npos) return false;
  clave = strTrim(l.substr(0, ig));
  valor = strTrim(l.substr(ig + 1));
  return !clave.empty();
}

/* las cadenas entre comillas de un valor («'a'», «['a', "b"]») */
std::vector<std::string> cadenas(const std::string &v) {
  std::vector<std::string> r;
  for (size_t i = 0; i < v.size(); i++) {
    const char c = v[i];
    if (c != '\'' && c != '"') continue;
    const size_t f = v.find(c, i + 1);
    if (f == std::string::npos) break;
    r.push_back(v.substr(i + 1, f - i - 1));
    i = f;
  }
  return r;
}

std::string sinComillas(const std::string &v) {
  const std::vector<std::string> c = cadenas(v);
  return c.empty() ? v : c.front();
}

/* un [[profile]]: de la linea `ini` (la cabecera) a `fin` (sin incluir) */
struct Bloque {
  size_t ini = 0, fin = 0;
  std::vector<std::string> juegos;   /* su active_in */
};

std::vector<Bloque> bloques(const std::vector<std::string> &lineas) {
  std::vector<Bloque> r;
  for (size_t i = 0; i < lineas.size(); i++) {
    if (sinComentario(lineas[i]) != "[[profile]]") continue;
    Bloque b;
    b.ini = i;
    size_t j = i + 1;
    while (j < lineas.size()) {
      const std::string l = sinComentario(lineas[j]);
      if (!l.empty() && l[0] == '[' && (l.rfind("[[", 0) == 0 || l.find('=') == std::string::npos)) break;   /* otra tabla */
      std::string k, v;
      if (parClave(lineas[j], k, v) && k == "active_in") {
        std::string todo = v;
        /* una lista que sigue en las lineas de abajo, hasta su «]» */
        if (!v.empty() && v[0] == '[' && v.find(']') == std::string::npos)
          while (j + 1 < lineas.size()) {
            j++;
            const std::string m = sinComentario(lineas[j]);
            todo += " " + m;
            if (m.find(']') != std::string::npos) break;
          }
        b.juegos = cadenas(todo);
      }
      j++;
    }
    b.fin = j;
    r.push_back(b);
    i = j - 1;
  }
  return r;
}

bool nombra(const Bloque &b, const std::string &exe) {
  for (const std::string &j : b.juegos)
    if (j == exe || strLower(j) == strLower(exe)) return true;
  return false;
}

bool leerLineas(std::vector<std::string> &lineas) {
  std::ifstream f(rutaConf());
  if (!f) return false;
  std::string l;
  while (std::getline(f, l)) lineas.push_back(l);
  return true;
}

std::string citar(const std::string &s) {
  if (s.find('\'') == std::string::npos) return "'" + s + "'";
  std::string r = "\"";
  for (char c : s) r += (c == '"' || c == '\\') ? std::string("\\") + c : std::string(1, c);
  return r + "\"";
}

/* `mako-cli validate -c`: cierto si valida o si no esta mako-cli (entonces no se puede mirar) */
bool valida(const std::string &ruta, std::string *err) {
  std::string orden = "timeout 15 mako-cli validate -c '";
  for (char c : ruta) orden += c == '\'' ? std::string("'\\''") : std::string(1, c);
  orden += "' 2>&1";
  std::string sal;
  const int rc = runCapture(orden, sal);
  if (rc == 127 || sal.find("not found") != std::string::npos) return true;   /* sin mako-cli */
  if (rc == 0 && sal.find("success") != std::string::npos) return true;
  if (err) *err = strTrim(sal.empty() ? std::string("mako-cli no la dio por buena") : sal);
  return false;
}

/* la copia de antes, una vez por proceso (y la primera de todas se queda) */
void respaldar(const std::string &ruta) {
  static bool hecho = false;
  if (hecho || !fileExists(ruta)) return;
  hecho = true;
  const std::string dir = rutaEstadoOdyssey("mako");
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  char fecha[32];
  const std::time_t t = std::time(nullptr);
  std::strftime(fecha, sizeof fecha, "%Y%m%d-%H%M%S", std::localtime(&t));
  std::filesystem::copy_file(ruta, dir + "/conf-" + fecha + ".toml", std::filesystem::copy_options::overwrite_existing, ec);
  if (!fileExists(dir + "/conf-antes-de-odysseyvk.toml"))
    std::filesystem::copy_file(ruta, dir + "/conf-antes-de-odysseyvk.toml", ec);
}

}  // namespace

bool instalado() {
  static const bool hay = [] {
    std::vector<std::string> dirs = {"/usr/share/vulkan/implicit_layer.d", "/usr/local/share/vulkan/implicit_layer.d",
                                     "/etc/vulkan/implicit_layer.d"};
    if (const char *h = std::getenv("HOME")) dirs.push_back(std::string(h) + "/.local/share/vulkan/implicit_layer.d");
    for (const std::string &d : dirs)
      if (fileExists(d + "/VkLayer_MAKO_render.json")) return true;
    return false;
  }();
  return hay;
}

/* ---- MAKO FG (ver mako.h) ---- */
const char *const kVersionGenerador = "1.24";

namespace {
/* la revision de Hugging Face que se midio el 27-nov (la de su etiqueta V1.24). Los SHA-256 son los de su MANIFEST.json
   (y los de MANIFEST.json y version.json, que no se nombran a si mismos, medidos al bajarlos ese dia) */
const char *const kOrigen = "https://huggingface.co/eugeniosegala/MAKO-FG/resolve/18d7d690515a658c993f84029b245cb710cb83f2";

bool programa(const std::string &ruta) {
  std::error_code ec;
  return !ruta.empty() && std::filesystem::is_regular_file(ruta, ec) && ::access(ruta.c_str(), X_OK) == 0;
}

/* la biblioteca de la capa de MAKO, por la `library_path` de su manifiesto: absoluta, junto al manifiesto o, con el nombre
   solo, donde la buscaria el cargador */
std::string bibliotecaMako() {
  std::vector<std::string> dirs = {"/usr/share/vulkan/implicit_layer.d", "/usr/local/share/vulkan/implicit_layer.d",
                                   "/etc/vulkan/implicit_layer.d"};
  if (const char *h = std::getenv("HOME")) dirs.push_back(std::string(h) + "/.local/share/vulkan/implicit_layer.d");
  for (const std::string &d : dirs) {
    std::string txt;
    if (!readSmallFile(d + "/VkLayer_MAKO_render.json", txt)) continue;
    const size_t k = txt.find("\"library_path\"");
    const size_t a = k == std::string::npos ? k : txt.find('"', txt.find(':', k) + 1);
    const size_t b = a == std::string::npos ? a : txt.find('"', a + 1);
    if (b == std::string::npos) continue;
    const std::string lib = txt.substr(a + 1, b - a - 1);
    std::vector<std::string> donde;
    if (!lib.empty() && lib[0] == '/') donde.push_back(lib);
    else if (lib.find('/') != std::string::npos) donde.push_back(d + "/" + lib);
    else
      for (const char *l : {"/usr/lib/", "/usr/lib64/", "/lib/", "/lib64/", "/usr/local/lib/"}) donde.push_back(l + lib);
    for (const std::string &r : donde)
      if (fileExists(r)) return r;
  }
  return "";
}
}  // namespace

const std::vector<ArchivoGenerador> &archivosGenerador() {
  static const std::vector<ArchivoGenerador> v = {
      {"runtime/x64/bin/mako-fg-host", "mako-fg-host", "ceba4c468280bdc458c33432dd7c420a85edc7f895ba3a3a8549a25aea173b4a",
       19102344, true},
      {"model/weights.bin", "model/weights.bin", "08ed3acfefa71a6d29a208e9a64c73d6593bfc2e83eb0230fe4f8d62ff7ea781", 914, false},
      {"model/ops.spv", "model/ops.spv", "431ec6d0224111435a62e9e1235d844cc9180083e9e5c79461b47d46a2b950ea", 53432, false},
      {"model/transport.spv", "model/transport.spv", "d2285a08b046fcb40ef63254589df3e1defa43e11e8f1026dd9a10c922db992d", 24412,
       false},
      {"model/mixture.spv", "model/mixture.spv", "f5395a370e03783273c68295c1b11215e8c0c7c978e07299b68d7e843d6c0e8e", 22392, false},
      {"LICENSE", "LICENSE", "e990112837dcd5af880065d292f22f394f87c69912f957eabc54dc0285c6a7fc", 2071, false},
      {"MODEL_CARD.md", "MODEL_CARD.md", "e35681cd07e8a18f204f5be744a81b1870309bf77e017d0f182bd6327da7de36", 24315, false},
      {"runtime/THIRD_PARTY_NOTICES", "THIRD_PARTY_NOTICES", "8595328bc0da204886179891b277fcf305b7cb7194f7e7672c929a1e2cd2ce10",
       61528, false},
      {"runtime/RUNTIME.json", "RUNTIME.json", "0f4e53d4f24efab90496461e078ded7cf853ddbf3d236035c5825abb147961c2", 1348, false},
      {"MANIFEST.json", "MANIFEST.json", "88cc7852d832d26c9e5fda1367fab2a3137e3d40805dd62080e607c1e02aaef3", 3841, false},
      {"version.json", "version.json", "a381de9c62e37a2e3b75c9c6afd0917e632ea901dd6b2d2d7f3115b75d4d08de", 679, false}};
  return v;
}

long bytesGenerador() {
  long t = 0;
  for (const ArchivoGenerador &a : archivosGenerador()) t += a.bytes;
  return t;
}

std::string origenGenerador() {
  if (const char *o = entornoOdyssey("ODYSSEYVK_MAKO_FG_ORIGEN"); o && *o) return o;   /* las pruebas: un file:// */
  return kOrigen;
}

std::string carpetaGenerador() {
  /* como las redes (carpetaRedes): bajo HOME, no XDG_DATA_HOME (las pruebas lo apuntan al de verdad para las capas) */
  const char *home = std::getenv("HOME");
  return std::string(home ? home : "/home") + "/.local/share/odysseyvk/mako-fg";
}

std::string hostSistema() {
  /* lo mismo que hostProgram() de MAKO, sin su $MAKO_FG_HOST: la carpeta D de su biblioteca y D/../lib/mako-fg,
     D/../../lib64/mako-fg y D/mako-fg */
  const std::string lib = bibliotecaMako();
  if (lib.empty()) return "";
  std::error_code ec;
  const std::filesystem::path d = std::filesystem::canonical(lib, ec).parent_path();
  if (ec) return "";
  for (const std::filesystem::path &c : {d.parent_path() / "lib/mako-fg/mako-fg-host",
                                         d.parent_path().parent_path() / "lib64/mako-fg/mako-fg-host", d / "mako-fg/mako-fg-host"})
    if (programa(c.string())) return c.string();
  return "";
}

std::string hostPropio() {
  /* entero: el programa con su tamano y la marca de la version que puso la instalacion al acabar (lo ultimo que escribe) */
  const std::string d = carpetaGenerador();
  const std::string h = d + "/mako-fg-host";
  struct stat sb {};
  if (!programa(h) || ::stat(h.c_str(), &sb) != 0 || (long)sb.st_size != archivosGenerador()[0].bytes) return "";
  std::string marca;
  if (!readSmallFile(d + "/instalado", marca) || marca.find(std::string("version = ") + kVersionGenerador) == std::string::npos)
    return "";
  return h;
}

std::string hostParaJuegos() {
  /* lo piden las lineas de Integraciones, una por juego: se vuelve a mirar como mucho cada 2 s */
  static std::string ultimo;
  static std::chrono::steady_clock::time_point cuando{};
  static bool mirado = false;
  const auto t = std::chrono::steady_clock::now();
  if (!mirado || t - cuando > std::chrono::seconds(2)) {
    ultimo = hostSistema().empty() ? hostPropio() : std::string();
    cuando = t, mirado = true;
  }
  return ultimo;
}

bool generadorInstalado() {
  if (const char *h = std::getenv("MAKO_FG_HOST"); h && *h && programa(h)) return true;
  return !hostSistema().empty() || !hostPropio().empty();
}

bool generadorEnJuego(const std::string &hostDelJuego) {
  /* MAKO toma $MAKO_FG_HOST tal cual, sin mirar si esta: con uno que no lleva a nada, no genera */
  if (!hostDelJuego.empty()) return programa(hostDelJuego);
  return !hostSistema().empty();
}

Instalacion leerInstalacion() {
  Instalacion r;
  std::string txt;
  if (!readSmallFile(rutaEstadoOdyssey("mako-fg"), txt)) return r;
  std::map<std::string, std::string> v;
  std::istringstream in(txt);
  std::string l, c, x;
  while (std::getline(in, l))
    if (parClave(l, c, x)) v[c] = x;
  const long pid = std::atol(v["pid"].c_str());
  r.pid = pid;
  r.hecho = std::atol(v["hecho"].c_str());
  r.total = std::atol(v["total"].c_str());
  r.hora = std::atol(v["hora"].c_str());
  if (v["estado"] == "bajando") {
    /* la que dice que baja tiene que seguir viva: si no, se corto (se mato, se apago el equipo) */
    std::string cmd;
    r.enMarcha = pid > 0 && ::kill((pid_t)pid, 0) == 0 && readSmallFile("/proc/" + std::to_string(pid) + "/cmdline", cmd) &&
                 cmd.find("mako") != std::string::npos;
    if (!r.enMarcha) r.error = idioma::tr("se cortó a medias: vuelve a intentarlo");
  } else if (v["estado"] == "lista") {
    r.lista = true;
  } else if (v["estado"] == "fallo") {
    /* el motivo ya viene en el idioma de la interfaz; el archivo y el detalle (un codigo, una ruta), tal cual */
    r.error = (v["archivo"].empty() ? std::string() : v["archivo"] + ": ") +
              (v["error"].empty() ? std::string(idioma::tr("la descarga falló")) : v["error"]) +
              (v["detalle"].empty() ? std::string() : " (" + v["detalle"] + ")");
  }
  return r;
}

std::string biblioteca() { return bibliotecaMako(); }

namespace {
std::map<std::string, std::string> leerExes() {
  std::map<std::string, std::string> m;
  std::string txt;
  if (!readSmallFile(rutaEstadoOdyssey("juegos-exe"), txt)) return m;
  std::istringstream in(txt);
  std::string l, c, v;
  while (std::getline(in, l)) {
    const size_t ig = l.find(" = ");
    if (ig != std::string::npos) m[strTrim(l.substr(0, ig))] = strTrim(l.substr(ig + 3));
  }
  return m;
}
}  // namespace

void apuntarExe(const std::string &appid, const std::string &exe) {
  if (appid.empty() || exe.empty() || exe.find('\n') != std::string::npos) return;
  std::map<std::string, std::string> m = leerExes();
  if (m[appid] == exe) return;
  m[appid] = exe;
  const std::string ruta = rutaEstadoOdyssey("juegos-exe"), tmp = ruta + ".tmp";
  if (FILE *f = std::fopen(tmp.c_str(), "we")) {
    std::fprintf(f, "# el ejecutable de cada juego de Steam (appid = exe), el que nombra su perfil de MAKO: lo apunta el visor\n");
    for (const auto &[k, x] : m) std::fprintf(f, "%s = %s\n", k.c_str(), x.c_str());
    std::fclose(f);
    ::rename(tmp.c_str(), ruta.c_str());
  }
}

std::string exeDeJuego(const std::string &appid, const std::string &alternativa) {
  const std::map<std::string, std::string> m = leerExes();
  if (const auto it = m.find(appid); it != m.end() && !it->second.empty()) return it->second;
  /* la lista de Neural Forge: «Stray.exe,Stray-Win64-Shipping.exe» (sin comillas) */
  std::vector<std::string> v;
  for (size_t i = 0; i < alternativa.size();) {
    const size_t j = std::min(alternativa.find(',', i), alternativa.size());
    std::string p = strTrim(alternativa.substr(i, j - i));
    p.erase(std::remove(p.begin(), p.end(), '"'), p.end());
    p.erase(std::remove(p.begin(), p.end(), '\''), p.end());
    if (!p.empty()) v.push_back(p);
    i = j + 1;
  }
  for (const std::string &x : v)
    if (x.find("Shipping") != std::string::npos) return x;
  return v.empty() ? std::string() : v.back();
}

bool lanzarInstalacion(std::string *err) {
  if (leerInstalacion().enMarcha) return true;
  return lanzarOrden("instalar", err);
}

bool lanzarOrden(const char *sub, std::string *err) {
  /* `odysseyvk mako instalar`, aparte y suelto (doble fork, su propia sesion): el que la lanza (el visor, el panel) no la
     espera ni la mata al cerrarse. El programa: este mismo si es `odysseyvk`; si no (el panel), el de su carpeta o el del PATH */
  std::string bin;
  {
    std::error_code ec;
    const std::filesystem::path yo = std::filesystem::read_symlink("/proc/self/exe", ec);
    if (!ec && yo.filename() == "odysseyvk") bin = yo.string();
    else if (!ec && fileExists((yo.parent_path() / "odysseyvk").string())) bin = (yo.parent_path() / "odysseyvk").string();
    else bin = "odysseyvk";
  }
  const std::string registro = rutaEstadoOdyssey("mako-fg.log");
  const pid_t p = ::fork();
  if (p < 0) {
    if (err) *err = std::string("no se pudo arrancar: ") + std::strerror(errno);
    return false;
  }
  if (p == 0) {
    ::setsid();
    if (::fork() != 0) ::_exit(0);
    const int nulo = ::open("/dev/null", O_RDONLY);
    /* anadiendo: un segundo clic seguido arranca otra que se para en el cerrojo, y no tiene que borrar lo de la primera */
    const int log = ::open(registro.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (nulo >= 0) ::dup2(nulo, 0);
    if (log >= 0) ::dup2(log, 1), ::dup2(log, 2);
    for (int fd = 3; fd < 1024; fd++) ::close(fd);
    ::execlp(bin.c_str(), bin.c_str(), "mako", sub, (char *)nullptr);
    ::_exit(127);
  }
  int st = 0;
  while (::waitpid(p, &st, 0) < 0 && errno == EINTR) {
  }
  return true;
}

std::string dllLossless() {
  /* la que diga [global] dll */
  std::vector<std::string> lineas;
  if (leerLineas(lineas)) {
    bool enGlobal = false;
    for (const std::string &l : lineas) {
      const std::string s = sinComentario(l);
      if (!s.empty() && s[0] == '[') enGlobal = s == "[global]";
      std::string k, v;
      if (enGlobal && parClave(l, k, v) && k == "dll") {
        const std::string r = sinComillas(v);
        if (!r.empty() && fileExists(r)) return r;
      }
    }
  }
  /* la carpeta de Lossless Scaling de cada biblioteca de Steam */
  const char *h = std::getenv("HOME");
  const std::string home = h ? h : "";
  std::vector<std::string> bibliotecas;
  for (const char *raiz : {"/.local/share/Steam", "/.steam/steam", "/.steam/debian-installation",
                           "/.var/app/com.valvesoftware.Steam/.local/share/Steam"}) {
    const std::string r = home + raiz;
    bibliotecas.push_back(r);
    std::string vdf;
    if (!readSmallFile(r + "/steamapps/libraryfolders.vdf", vdf)) continue;
    for (size_t p = vdf.find("\"path\""); p != std::string::npos; p = vdf.find("\"path\"", p + 6)) {
      const size_t a = vdf.find('"', p + 6), b = a == std::string::npos ? a : vdf.find('"', a + 1);
      if (a != std::string::npos && b != std::string::npos) bibliotecas.push_back(vdf.substr(a + 1, b - a - 1));
    }
  }
  for (const std::string &b : bibliotecas) {
    const std::string r = b + "/steamapps/common/Lossless Scaling/Lossless.dll";
    if (fileExists(r)) return r;
  }
  return {};
}

std::string rutaConf() {
  if (const char *c = std::getenv("MAKO_CONFIG"); c && *c) return c;
  const char *xc = std::getenv("XDG_CONFIG_HOME");
  const char *h = std::getenv("HOME");
  return (xc && *xc ? std::string(xc) : std::string(h ? h : "") + "/.config") + "/mako-render/conf.toml";
}

Perfil leer(const std::string &exe) {
  Perfil p;
  std::vector<std::string> lineas;
  if (exe.empty() || !leerLineas(lineas)) return p;
  for (const Bloque &b : bloques(lineas)) {
    if (!nombra(b, exe)) continue;
    p.existe = true;
    for (size_t i = b.ini + 1; i < b.fin; i++) {
      std::string k, v;
      if (!parClave(lineas[i], k, v)) continue;
      if (k == "name") p.nombre = sinComillas(v);
      else if (k == "frame_generation_enabled") p.puesta = v == "true";
      else if (k == "multiplier") p.multiplicador = std::atoi(v.c_str());
      else if (k == "flow_scale") p.flujo = std::atof(v.c_str());
      else if (k == "performance_mode") p.rendimiento = v == "true";
      else if (k == "gpu") p.tarjeta = sinComillas(v);
      else if (k == "frame_generation_model") p.modelo = sinComillas(v);
    }
    break;
  }
  if (p.modelo.empty()) p.modelo = "mako-fg";   /* lo de fabrica de MAKO */
  return p;
}

bool fijar(const std::string &exe, const std::map<std::string, std::string> &claves, std::string *err) {
  if (exe.empty()) {
    if (err) *err = "no se sabe el ejecutable del juego";
    return false;
  }
  const std::string ruta = rutaConf();
  std::vector<std::string> lineas;
  const bool habia = leerLineas(lineas);
  if (!habia) lineas = {"version = 2", ""};
  const std::vector<Bloque> bs = bloques(lineas);
  const Bloque *b = nullptr;
  for (const Bloque &x : bs)
    if (nombra(x, exe)) {
      b = &x;
      break;
    }
  if (b) {
    /* el perfil que ya nombra al juego: cada clave en su linea, o detras de `name` (o de la cabecera) */
    size_t fin = b->fin, tras = b->ini + 1;
    for (size_t i = b->ini + 1; i < fin; i++) {
      std::string k, v;
      if (parClave(lineas[i], k, v) && k == "name") tras = i + 1;
    }
    for (const auto &[clave, valor] : claves) {
      bool puesta = false;
      for (size_t i = b->ini + 1; i < fin; i++) {
        std::string k, v;
        if (parClave(lineas[i], k, v) && k == clave) {
          if (valor.empty()) {
            lineas.erase(lineas.begin() + (long)i);   /* quitarla */
            fin--;
            if (tras > i) tras--;
          } else {
            lineas[i] = clave + " = " + valor;
          }
          puesta = true;
          break;
        }
      }
      if (!puesta && !valor.empty()) {
        lineas.insert(lineas.begin() + (long)tras, clave + " = " + valor);
        fin++, tras++;
      }
    }
  } else {
    /* uno nuevo, al final */
    if (!lineas.empty() && !strTrim(lineas.back()).empty()) lineas.push_back("");
    lineas.push_back("[[profile]]");
    lineas.push_back("name = " + citar("OdysseyVK · " + exe));
    lineas.push_back("active_in = " + citar(exe));
    if (!claves.count("frame_generation_provisioned")) lineas.push_back("frame_generation_provisioned = true");
    for (const auto &[clave, valor] : claves)
      if (!valor.empty()) lineas.push_back(clave + " = " + valor);
  }
  std::string texto;
  for (const std::string &l : lineas) texto += l + "\n";
  /* aparte, comprobado, y despues de golpe */
  std::error_code ec;
  std::filesystem::create_directories(std::filesystem::path(ruta).parent_path(), ec);
  const std::string tmp = ruta + ".odysseyvk-nuevo";
  {
    std::ofstream o(tmp, std::ios::trunc);
    if (!o) {
      if (err) *err = "no se puede escribir " + tmp + ": " + std::strerror(errno);
      return false;
    }
    o << texto;
  }
  if (!valida(tmp, err)) {
    std::remove(tmp.c_str());
    return false;
  }
  struct stat sb {};
  if (habia && ::stat(ruta.c_str(), &sb) == 0) ::chmod(tmp.c_str(), sb.st_mode & 07777);
  else ::chmod(tmp.c_str(), 0600);
  respaldar(ruta);
  if (std::rename(tmp.c_str(), ruta.c_str()) != 0) {
    if (err) *err = std::string("no se pudo cambiar el fichero: ") + std::strerror(errno);
    std::remove(tmp.c_str());
    return false;
  }
  return true;
}

}  // namespace mako
